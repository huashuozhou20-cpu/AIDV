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
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class NestedLoopJoinExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> left_;
    std::unique_ptr<AbstractExecutor> right_;
    size_t len_;
    std::vector<ColMeta> cols_;

    std::vector<Condition> fed_conds_;
    bool is_end_;
    std::unique_ptr<RmRecord> left_record_;
    std::unique_ptr<RmRecord> right_record_;
    JoinType join_type_;
    bool left_has_match_;         // current left row found at least one match
    bool null_pad_output_;        // outputting left + NULL (no match case)
    std::unique_ptr<RmRecord> null_right_;
    // FULL_JOIN: track which right rows had matches, emit unmatched after left exhausted
    size_t right_scan_pos_;
    std::vector<bool> right_matched_;
    bool left_exhausted_;
    std::unique_ptr<RmRecord> null_left_;

    bool eval_cond(const Condition &cond, const RmRecord &left_rec, const RmRecord &right_rec) {
        if (cond.op == OP_OR) {
            for (auto &child : cond.children) {
                if (eval_cond(child, left_rec, right_rec)) return true;
            }
            return cond.children.empty();
        }
        if (cond.op == OP_NOT) {
            for (auto &child : cond.children) {
                if (eval_cond(child, left_rec, right_rec)) return false;
            }
            return true;
        }

        ColMeta lhs_col_meta = get_col_meta(cond.lhs_col);
        char *lhs_data = nullptr;
        if (!lhs_col_meta.tab_name.empty()) {
            lhs_data = (lhs_col_meta.tab_name == left_->cols()[0].tab_name)
                           ? left_rec.data + lhs_col_meta.offset
                           : right_rec.data + lhs_col_meta.offset;
        }

        if (cond.op == OP_IS_NULL)
            return check_is_null(lhs_data, lhs_col_meta.type, lhs_col_meta.len);
        if (cond.op == OP_IS_NOT_NULL)
            return !check_is_null(lhs_data, lhs_col_meta.type, lhs_col_meta.len);

        const auto &rhs_col_meta = get_col_meta(cond.rhs_col);
        char *rhs_data = nullptr;
        if (!rhs_col_meta.tab_name.empty()) {
            rhs_data = (rhs_col_meta.tab_name == left_->cols()[0].tab_name)
                           ? left_rec.data + rhs_col_meta.offset
                           : right_rec.data + rhs_col_meta.offset;
        }

        if (lhs_col_meta.type != rhs_col_meta.type) return false;

        int cmp = 0;
        switch (lhs_col_meta.type) {
            case TYPE_INT: {
                int a = *(int *)lhs_data, b = *(int *)rhs_data;
                cmp = (a < b) ? -1 : ((a > b) ? 1 : 0);
                break;
            }
            case TYPE_FLOAT: {
                float a = *(float *)lhs_data, b = *(float *)rhs_data;
                cmp = (a < b) ? -1 : ((a > b) ? 1 : 0);
                break;
            }
            case TYPE_STRING:
                cmp = memcmp(lhs_data, rhs_data, lhs_col_meta.len);
                break;
        }

        switch (cond.op) {
            case OP_EQ: return cmp == 0;
            case OP_NE: return cmp != 0;
            case OP_LT: return cmp < 0;
            case OP_GT: return cmp > 0;
            case OP_LE: return cmp <= 0;
            case OP_GE: return cmp >= 0;
            default: return false;
        }
    }

    ColMeta get_col_meta(const TabCol &target) {
        // Search child columns first for correct original offsets
        for (auto &col : left_->cols()) {
            if (col.tab_name == target.tab_name && col.name == target.col_name) return col;
        }
        for (auto &col : right_->cols()) {
            if (col.tab_name == target.tab_name && col.name == target.col_name) return col;
        }
        // Fallback: combined cols_ (right offsets shifted by left len)
        for (auto &col : cols_) {
            if (col.tab_name == target.tab_name && col.name == target.col_name) return col;
        }
        return ColMeta{};
    }

    bool check_all_conds() {
        if (fed_conds_.empty()) return true;
        for (auto &cond : fed_conds_) {
            if (!eval_cond(cond, *left_record_, *right_record_)) return false;
        }
        return true;
    }

    // Find next matching pair. Returns true if match found.
    // Handles outer join: when right exhausted for current left, emits NULL-padded left if needed.
    // Tracks matched right row positions for FULL_JOIN unmatched-right phase.
    bool find_next_match() {
        while (true) {
            right_->nextTuple();
            if (!right_->is_end()) {
                right_record_ = right_->Next();
                right_scan_pos_++;
                if (check_all_conds()) {
                    left_has_match_ = true;
                    null_pad_output_ = false;
                    if (join_type_ == FULL_JOIN) {
                        if (right_scan_pos_ >= right_matched_.size())
                            right_matched_.resize(right_scan_pos_ + 1, false);
                        right_matched_[right_scan_pos_] = true;
                    }
                    return true;
                }
                continue;
            }

            // Right exhausted. Check if current left row had any match.
            if (!left_has_match_ && (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN)) {
                if (!null_right_)
                    null_right_ = std::make_unique<RmRecord>(right_->tupleLen());
                memset(null_right_->data, 0, right_->tupleLen());
                null_pad_output_ = true;
                left_has_match_ = true;  // mark so we advance on next call
                return true;
            }

            // Advance to next left row
            left_->nextTuple();
            if (left_->is_end()) return false;
            left_record_ = left_->Next();
            left_has_match_ = false;
            null_pad_output_ = false;

            // Reset right scan
            right_->beginTuple();
            right_scan_pos_ = 0;
            if (!right_->is_end()) {
                right_record_ = right_->Next();
                right_scan_pos_++;
                if (check_all_conds()) {
                    left_has_match_ = true;
                    if (join_type_ == FULL_JOIN) {
                        if (right_scan_pos_ >= right_matched_.size())
                            right_matched_.resize(right_scan_pos_ + 1, false);
                        right_matched_[right_scan_pos_] = true;
                    }
                    return true;
                }
            }
        }
    }

    // After main loop: emit unmatched right rows for FULL_JOIN.
    bool find_unmatched_right() {
        while (true) {
            right_->nextTuple();
            if (right_->is_end()) return false;
            right_record_ = right_->Next();
            right_scan_pos_++;
            if (right_scan_pos_ >= right_matched_.size() || !right_matched_[right_scan_pos_]) {
                return true;
            }
        }
    }

   public:
    NestedLoopJoinExecutor(std::unique_ptr<AbstractExecutor> left, std::unique_ptr<AbstractExecutor> right,
                            std::vector<Condition> conds, JoinType join_type = INNER_JOIN)
        : join_type_(join_type), left_has_match_(false), null_pad_output_(false),
          right_scan_pos_(0), left_exhausted_(false) {
        left_ = std::move(left);
        right_ = std::move(right);
        len_ = left_->tupleLen() + right_->tupleLen();
        cols_ = left_->cols();
        auto right_cols = right_->cols();
        for (auto &col : right_cols) {
            col.offset += left_->tupleLen();
        }
        cols_.insert(cols_.end(), right_cols.begin(), right_cols.end());
        is_end_ = true;
        fed_conds_ = std::move(conds);
    }

    void beginTuple() override {
        left_exhausted_ = false;
        right_scan_pos_ = 0;
        if (join_type_ == FULL_JOIN) right_matched_.clear();

        left_->beginTuple();
        if (left_->is_end()) {
            // Left table empty. For FULL_JOIN, all right rows are unmatched.
            if (join_type_ == FULL_JOIN) {
                start_unmatched_right_phase();
                return;
            }
            is_end_ = true;
            return;
        }
        left_record_ = left_->Next();
        left_has_match_ = false;
        null_pad_output_ = false;

        right_->beginTuple();
        right_scan_pos_ = 0;
        if (!right_->is_end()) {
            right_record_ = right_->Next();
            right_scan_pos_++;
            if (check_all_conds()) {
                left_has_match_ = true;
                if (join_type_ == FULL_JOIN) {
                    if (right_scan_pos_ >= right_matched_.size())
                        right_matched_.resize(right_scan_pos_ + 1, false);
                    right_matched_[right_scan_pos_] = true;
                }
                is_end_ = false;
                return;
            }
        }

        if (find_next_match()) {
            is_end_ = false;
        } else if (join_type_ == FULL_JOIN) {
            start_unmatched_right_phase();
        } else {
            is_end_ = true;
        }
    }

    void start_unmatched_right_phase() {
        left_exhausted_ = true;
        if (!null_left_)
            null_left_ = std::make_unique<RmRecord>(left_->tupleLen());
        memset(null_left_->data, 0, left_->tupleLen());
        right_->beginTuple();
        right_scan_pos_ = 0;
        if (find_unmatched_right()) {
            is_end_ = false;
        } else {
            is_end_ = true;
        }
    }

    void nextTuple() override {
        if (left_exhausted_) {
            // Phase 2: emitting unmatched right rows
            if (!find_unmatched_right()) {
                is_end_ = true;
            }
            return;
        }

        // If we just emitted a NULL-padded left row (no match case), advance left first
        if (null_pad_output_) {
            left_->nextTuple();
            if (left_->is_end()) {
                if (join_type_ == FULL_JOIN) {
                    start_unmatched_right_phase();
                } else {
                    is_end_ = true;
                }
                return;
            }
            left_record_ = left_->Next();
            left_has_match_ = false;
            null_pad_output_ = false;

            right_->beginTuple();
            right_scan_pos_ = 0;
            if (!right_->is_end()) {
                right_record_ = right_->Next();
                right_scan_pos_++;
                if (check_all_conds()) {
                    left_has_match_ = true;
                    if (join_type_ == FULL_JOIN) {
                        if (right_scan_pos_ >= right_matched_.size())
                            right_matched_.resize(right_scan_pos_ + 1, false);
                        right_matched_[right_scan_pos_] = true;
                    }
                    return;
                }
            }
        }

        if (!find_next_match()) {
            if (join_type_ == FULL_JOIN) {
                start_unmatched_right_phase();
            } else {
                is_end_ = true;
            }
        }
    }

    bool is_end() const override { return is_end_; }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end_) return nullptr;
        auto rec = std::make_unique<RmRecord>(len_);
        if (left_exhausted_) {
            // FULL_JOIN phase 2: NULL left + right
            memset(rec->data, 0, left_->tupleLen());
            memcpy(rec->data + left_->tupleLen(), right_record_->data, right_->tupleLen());
        } else if (null_pad_output_) {
            memcpy(rec->data, left_record_->data, left_->tupleLen());
            memset(rec->data + left_->tupleLen(), 0, right_->tupleLen());
        } else {
            memcpy(rec->data, left_record_->data, left_->tupleLen());
            memcpy(rec->data + left_->tupleLen(), right_record_->data, right_->tupleLen());
        }
        return rec;
    }

    Rid &rid() override { return _abstract_rid; }
};
