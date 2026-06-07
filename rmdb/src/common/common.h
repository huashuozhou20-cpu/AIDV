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
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "defs.h"
#include "record/rm_defs.h"

constexpr int NULL_INT = 0x80000000;     // INT32_MIN sentinel for NULL
constexpr float NULL_FLOAT = 0.0f;       // (unused placeholder)


struct TabCol {
    std::string tab_name;
    std::string col_name;

    friend bool operator<(const TabCol &x, const TabCol &y) {
        return std::make_pair(x.tab_name, x.col_name) < std::make_pair(y.tab_name, y.col_name);
    }
};

struct Value {
    ColType type;  // type of value
    union {
        int int_val;      // int value
        float float_val;  // float value
    };
    std::string str_val;  // string value
    bool is_null_ = false;

    std::shared_ptr<RmRecord> raw;  // raw record buffer

    void set_int(int int_val_) {
        type = TYPE_INT;
        int_val = int_val_;
    }

    void set_float(float float_val_) {
        type = TYPE_FLOAT;
        float_val = float_val_;
    }

    void set_str(std::string str_val_) {
        type = TYPE_STRING;
        str_val = std::move(str_val_);
    }

    void set_null() {
        is_null_ = true;
        type = TYPE_INT;  // placeholder, will be overwritten by actual column type
        int_val = NULL_INT;
    }

    void init_raw(int len) {
        if (raw != nullptr) return;  // already initialized
        if (is_null_) {
            // Create NULL sentinel for the actual column type at the given length
            raw = std::make_shared<RmRecord>(len);
            memset(raw->data, 0, len);
            return;
        }
        raw = std::make_shared<RmRecord>(len);
        if (type == TYPE_INT) {
            assert(len == sizeof(int));
            *(int *)(raw->data) = int_val;
        } else if (type == TYPE_FLOAT) {
            assert(len == sizeof(float));
            *(float *)(raw->data) = float_val;
        } else if (type == TYPE_STRING) {
            if (len < (int)str_val.size()) {
                throw StringOverflowError();
            }
            memset(raw->data, 0, len);
            memcpy(raw->data, str_val.c_str(), str_val.size());
        }
    }
};

enum CompOp {
    OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_IS_NULL, OP_IS_NOT_NULL,
    OP_LIKE, OP_NOT_LIKE,
    OP_BETWEEN, OP_NOT_BETWEEN,
    OP_IN, OP_NOT_IN,
    OP_OR, OP_NOT
};

struct Condition {
    // Leaf: comparison data
    TabCol lhs_col;   // left-hand side column
    CompOp op;        // comparison operator
    bool is_rhs_val;  // true if right-hand side is a value (not a column)
    TabCol rhs_col;   // right-hand side column
    Value rhs_val;    // right-hand side value (also used for LIKE pattern)
    Value rhs_val2;   // BETWEEN high value
    std::vector<Value> in_values;  // IN value list
    bool is_arith_expr = false;  // LHS is an arithmetic expression → no index scan

    // Internal node: OR / NOT children
    std::vector<Condition> children;

    bool is_leaf() const { return children.empty(); }
};

struct SetClause {
    TabCol lhs;
    Value rhs;
};

// Check if a data value represents SQL NULL
inline bool check_is_null(const char *data, ColType type, int len) {
    switch (type) {
        case TYPE_INT: return *(const int *)data == NULL_INT;
        case TYPE_FLOAT: return *(const float *)data == NULL_FLOAT;
        case TYPE_STRING: return data[0] == '\0';
    }
    return false;
}

// Simple LIKE pattern matching with % and _
inline bool like_match(const std::string &str, const std::string &pattern) {
    auto match = [](const std::string &s, const std::string &p) -> bool {
        auto impl = [&](auto &self, const char *s_ptr, const char *p_ptr) -> bool {
            if (*p_ptr == '\0') return *s_ptr == '\0';
            if (*p_ptr == '%') {
                if (self(self, s_ptr, p_ptr + 1)) return true;
                if (*s_ptr == '\0') return false;
                return self(self, s_ptr + 1, p_ptr);
            }
            if (*s_ptr == '\0') return false;
            if (*p_ptr == '_' || *p_ptr == *s_ptr)
                return self(self, s_ptr + 1, p_ptr + 1);
            return false;
        };
        return impl(impl, s.c_str(), p.c_str());
    };
    return match(str, pattern);
}

// 查询结果，用于格式化返回给客户端
struct Result {
    enum Status { SUCCESS = 0, ABORT = 1, FAILURE = 2 };
    Status status;
    std::string msg;  // 结果数据或错误信息
};