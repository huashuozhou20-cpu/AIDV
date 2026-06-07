/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include <cassert>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include "common/context.h"
#include "common/config.h"

#define RECORD_COUNT_LENGTH 40

// ── CJK-aware display-width helpers ─────────────────────────────────────

// Returns the terminal-column display width of a UTF-8 string.
// ASCII chars = width 1, multi-byte lead bytes (CJK, etc.) = width 2.
static int utf8_display_width(const std::string &s) {
    int w = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80)
            w += 1;                     // ASCII
        else if ((c & 0xC0) != 0x80)
            w += 2;                     // lead byte of a multi-byte sequence
        // continuation bytes (0x80-0xBF) add nothing
    }
    return w;
}

// Truncate a UTF-8 string to at most max_w display columns, without
// splitting a multi-byte sequence.  Appends "..." if truncation occurs.
static std::string utf8_truncate(const std::string &s, int max_w) {
    int w = 0;
    size_t last_good = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int seq_len = 1;
        if (c >= 0xC0) {
            if      ((c & 0xE0) == 0xC0) seq_len = 2;
            else if ((c & 0xF0) == 0xE0) seq_len = 3;
            else if ((c & 0xF8) == 0xF0) seq_len = 4;
            else seq_len = 1;  // invalid, treat as single byte
        }
        int char_w = (c < 0x80) ? 1 : 2;
        if (w + char_w > max_w) break;
        if (i + seq_len > s.size()) break;
        w += char_w;
        last_good = i + seq_len;
        i += seq_len;
    }
    if (last_good < s.size())
        return s.substr(0, last_good) + "...";
    return s;
}

// Right-pad a string to a target display width (replaces std::setw which
// counts bytes, not terminal columns).
static std::string utf8_pad(const std::string &s, int target_w) {
    int cur_w = utf8_display_width(s);
    if (cur_w >= target_w) return s;
    return s + std::string(target_w - cur_w, ' ');
}

class RecordPrinter {
    static constexpr size_t COL_WIDTH = 16;
    size_t num_cols;
public:
    RecordPrinter(size_t num_cols_) : num_cols(num_cols_) {
        assert(num_cols_ > 0);
    }

    void print_separator(Context *context) const {
        for (size_t i = 0; i < num_cols; i++) {
            // std::cout << '+' << std::string(COL_WIDTH + 2, '-');
            std::string str = "+" + std::string(COL_WIDTH + 2, '-');
            if(context->ellipsis_ == false && *context->offset_ + RECORD_COUNT_LENGTH + str.length() < BUFFER_LENGTH) {
                memcpy(context->data_send_ + *(context->offset_), str.c_str(), str.length());
                *(context->offset_) = *(context->offset_) + str.length();
            }
            else {
                context->ellipsis_ = true;
            }
        }
        std::string str = "+\n";
        if(context->ellipsis_ == false && *context->offset_ + RECORD_COUNT_LENGTH + str.length() < BUFFER_LENGTH) {
            memcpy(context->data_send_ + *(context->offset_), str.c_str(), str.length());
            *(context->offset_) = *(context->offset_) + str.length();
        }
        else {
            context->ellipsis_ = true;
        }
    }

    void print_record(const std::vector<std::string> &rec_str, Context *context) const {
        assert(rec_str.size() == num_cols);
        for (auto col: rec_str) {
            if (utf8_display_width(col) > (int)COL_WIDTH) {
                col = utf8_truncate(col, (int)COL_WIDTH - 3);
            }
            // std::cout << "| " << std::setw(COL_WIDTH) << col << ' ';
            std::stringstream ss;
            ss << "| " << utf8_pad(col, COL_WIDTH) << " ";
            if(context->ellipsis_ == false && *context->offset_ + RECORD_COUNT_LENGTH + ss.str().length() < BUFFER_LENGTH) {
                memcpy(context->data_send_ + *(context->offset_), ss.str().c_str(), ss.str().length());
                *(context->offset_) = *(context->offset_) + ss.str().length();
            }
            else {
                context->ellipsis_ = true;
            }
        }
        // std::cout << "|\n";
        std::string str = "|\n";
        if(context->ellipsis_ == false && *context->offset_ + RECORD_COUNT_LENGTH + str.length() < BUFFER_LENGTH) {
            memcpy(context->data_send_ + *(context->offset_), str.c_str(), str.length());
            *(context->offset_) = *(context->offset_) + str.length();
        }
    }

    static void print_record_count(size_t num_rec, Context *context) {
        // std::cout << "Total record(s): " << num_rec << '\n';
        std::string str = "";
        if(context->ellipsis_ == true) {
            str = "... ...\n";
        }
        str += "Total record(s): " + std::to_string(num_rec) + '\n';
        memcpy(context->data_send_ + *(context->offset_), str.c_str(), str.length());
        *(context->offset_) = *(context->offset_) + str.length();
    }
};
