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

#include <algorithm>
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "execution_sort.h"
#include "index/ix.h"
#include "system/sm.h"

class SortMergeJoinExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> left_;
    std::unique_ptr<AbstractExecutor> right_;
    size_t len_;
    std::vector<ColMeta> cols_;

    JoinType join_type_;
    std::vector<Condition> conds_;
    std::vector<Condition> equi_conds_;
    bool is_end_;
    std::unique_ptr<RmRecord> left_record_;
    // Pre-allocated records — reused across calls to avoid per-row heap
    // allocations and the resulting ptmalloc lock contention under concurrency.
    RmRecord right_record_;
    RmRecord null_right_;
    RmRecord null_left_;
    RmRecord output_record_;
    struct RightMatch {
        RmRecord rec;
        size_t pos;
    };
    std::vector<RightMatch> right_matches_;
    size_t right_match_pos_;
    size_t right_position_;
    bool left_has_group_match_;
    bool null_pad_output_;
    std::vector<bool> right_row_matched_;
    size_t right_row_count_;
    bool left_exhausted_;

    // ---- helpers ----

    ColMeta get_col_meta(const TabCol &target) {
        for (auto &col : cols_) {
            if (col.tab_name == target.tab_name && col.name == target.col_name)
                return col;
        }
        for (auto &col : left_->cols()) {
            if (col.tab_name == target.tab_name && col.name == target.col_name)
                return col;
        }
        for (auto &col : right_->cols()) {
            if (col.tab_name == target.tab_name && col.name == target.col_name)
                return col;
        }
        return ColMeta{};
    }

    int compare_keys(const RmRecord &left_rec, const RmRecord &right_rec) {
        for (auto &cond : equi_conds_) {
            ColMeta lhs_meta = get_col_meta(cond.lhs_col);
            ColMeta rhs_meta = get_col_meta(cond.rhs_col);

            char *lhs_data = (lhs_meta.tab_name == left_->cols()[0].tab_name)
                                 ? left_rec.data + lhs_meta.offset
                                 : right_rec.data + lhs_meta.offset;
            char *rhs_data = (rhs_meta.tab_name == left_->cols()[0].tab_name)
                                 ? left_rec.data + rhs_meta.offset
                                 : right_rec.data + rhs_meta.offset;

            int cmp = 0;
            switch (lhs_meta.type) {
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
                    cmp = memcmp(lhs_data, rhs_data, lhs_meta.len);
                    break;
            }
            if (cmp != 0) return cmp;
        }
        return 0;
    }

    bool eval_cond(const Condition &cond, const RmRecord &left_rec,
                   const RmRecord &right_rec) {
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

        ColMeta lhs_meta = get_col_meta(cond.lhs_col);
        char *lhs_data = nullptr;
        if (!lhs_meta.tab_name.empty()) {
            lhs_data = (lhs_meta.tab_name == left_->cols()[0].tab_name)
                           ? left_rec.data + lhs_meta.offset
                           : right_rec.data + lhs_meta.offset;
        }

        if (cond.op == OP_IS_NULL)
            return check_is_null(lhs_data, lhs_meta.type, lhs_meta.len);
        if (cond.op == OP_IS_NOT_NULL)
            return !check_is_null(lhs_data, lhs_meta.type, lhs_meta.len);

        ColMeta rhs_meta = get_col_meta(cond.rhs_col);
        char *rhs_data = nullptr;
        if (!rhs_meta.tab_name.empty()) {
            rhs_data = (rhs_meta.tab_name == left_->cols()[0].tab_name)
                           ? left_rec.data + rhs_meta.offset
                           : right_rec.data + rhs_meta.offset;
        }

        if (lhs_meta.type != rhs_meta.type) return false;

        int cmp = 0;
        switch (lhs_meta.type) {
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
                cmp = memcmp(lhs_data, rhs_data, lhs_meta.len);
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

    bool check_all_conds() {
        if (conds_.empty()) return true;
        for (auto &cond : conds_) {
            if (!eval_cond(cond, *left_record_, right_record_)) return false;
        }
        return true;
    }

    void mark_right_match(size_t pos) {
        if (join_type_ == FULL_JOIN) {
            if (pos >= right_row_matched_.size())
                right_row_matched_.resize(pos + 1, false);
            right_row_matched_[pos] = true;
        }
    }

    // Copy a buffered RightMatch record into the reusable right_record_.
    void set_right_record(const RmRecord &src) {
        memcpy(right_record_.data, src.data, right_record_.size);
    }

    // Zero-fill the reusable null_right_ record.
    void zero_right_record() { memset(right_record_.data, 0, right_record_.size); }

    bool advance_to_next_match() {
        while (true) {
            if (right_match_pos_ < right_matches_.size()) {
                set_right_record(right_matches_[right_match_pos_].rec);
                size_t matched_pos = right_matches_[right_match_pos_].pos;
                right_match_pos_++;
                if (check_all_conds()) {
                    left_has_group_match_ = true;
                    null_pad_output_ = false;
                    mark_right_match(matched_pos);
                    return true;
                }
                continue;
            }

            if (!left_has_group_match_ && right_matches_.empty() &&
                (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN)) {
                zero_right_record();
                null_pad_output_ = true;
                return true;
            }

            right_matches_.clear();
            right_match_pos_ = 0;
            left_has_group_match_ = false;

            left_->nextTuple();
            if (left_->is_end()) {
                return false;
            }
            left_record_ = left_->Next();

            while (!right_->is_end()) {
                auto rec = right_->Next();
                int cmp = compare_keys(*left_record_, *rec);
                if (cmp < 0) break;
                right_position_++;
                right_matches_.push_back({*rec, right_position_});
                right_->nextTuple();
            }
            size_t i = 0;
            while (i < right_matches_.size()) {
                int cmp = compare_keys(*left_record_, right_matches_[i].rec);
                if (cmp == 0) break;
                i++;
            }
            right_matches_.erase(right_matches_.begin(), right_matches_.begin() + i);

            if (!right_matches_.empty()) {
                for (size_t j = 0; j < right_matches_.size(); j++) {
                    set_right_record(right_matches_[j].rec);
                    if (check_all_conds()) {
                        left_has_group_match_ = true;
                        null_pad_output_ = false;
                        right_match_pos_ = j + 1;
                        mark_right_match(right_matches_[j].pos);
                        return true;
                    }
                }
                if (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN) {
                    zero_right_record();
                    null_pad_output_ = true;
                    return true;
                }
                continue;
            }

            if (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN) {
                zero_right_record();
                null_pad_output_ = true;
                return true;
            }

            right_->beginTuple();
            right_position_ = 0;
            if (right_->is_end()) return false;
        }
    }

    bool find_unmatched_right() {
        while (true) {
            right_->nextTuple();
            if (right_->is_end()) return false;
            auto rec = right_->Next();
            // Copy into reusable right_record_
            memcpy(right_record_.data, rec->data, right_record_.size);
            right_row_count_++;
            if (right_row_count_ >= right_row_matched_.size() ||
                !right_row_matched_[right_row_count_]) {
                return true;
            }
        }
    }

   public:
    SortMergeJoinExecutor(std::unique_ptr<AbstractExecutor> left,
                          std::unique_ptr<AbstractExecutor> right,
                          std::vector<Condition> conds,
                          std::vector<Condition> equi_conds,
                          JoinType join_type = INNER_JOIN)
        : join_type_(join_type), conds_(std::move(conds)), equi_conds_(std::move(equi_conds)),
          right_position_(0), left_has_group_match_(false), null_pad_output_(false),
          right_row_count_(0), left_exhausted_(false) {

        if (!equi_conds_.empty()) {
            auto left_sort_col = equi_conds_[0].lhs_col;
            auto right_sort_col = equi_conds_[0].rhs_col;
            if (left_sort_col.tab_name != left->cols()[0].tab_name)
                std::swap(left_sort_col, right_sort_col);

            left_ = std::make_unique<SortExecutor>(std::move(left), left_sort_col, false);
            right_ = std::make_unique<SortExecutor>(std::move(right), right_sort_col, false);
        } else {
            left_ = std::move(left);
            right_ = std::move(right);
        }

        len_ = left_->tupleLen() + right_->tupleLen();
        cols_ = left_->cols();
        auto right_cols = right_->cols();
        for (auto &col : right_cols) {
            col.offset += left_->tupleLen();
        }
        cols_.insert(cols_.end(), right_cols.begin(), right_cols.end());
        is_end_ = true;
        right_match_pos_ = 0;

        // Pre-allocate reusable records to eliminate per-row heap allocations
        // and the resulting ptmalloc lock contention under concurrency.
        size_t rlen = right_->tupleLen();
        right_record_ = RmRecord(static_cast<int>(rlen));
        null_right_   = RmRecord(static_cast<int>(rlen));
        null_left_    = RmRecord(static_cast<int>(left_->tupleLen()));
        output_record_ = RmRecord(static_cast<int>(len_));
    }

    void beginTuple() override {
        left_exhausted_ = false;
        right_row_count_ = 0;
        if (join_type_ == FULL_JOIN) right_row_matched_.clear();
        left_has_group_match_ = false;
        null_pad_output_ = false;

        left_->beginTuple();
        if (left_->is_end()) {
            if (join_type_ == FULL_JOIN) {
                start_unmatched_right_phase();
                return;
            }
            is_end_ = true;
            return;
        }
        left_record_ = left_->Next();

        right_->beginTuple();
        right_matches_.clear();
        right_match_pos_ = 0;
        right_position_ = 0;

        while (!right_->is_end()) {
            auto rec = right_->Next();
            int cmp = compare_keys(*left_record_, *rec);
            if (cmp < 0) {
                if (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN) {
                    zero_right_record();
                    null_pad_output_ = true;
                    is_end_ = false;
                    return;
                }
                left_->nextTuple();
                if (left_->is_end()) { is_end_ = true; return; }
                left_record_ = left_->Next();
                continue;
            }
            if (cmp > 0) {
                right_->nextTuple();
                continue;
            }
            right_position_++;
            right_matches_.push_back({*rec, right_position_});
            while (!right_->is_end()) {
                right_->nextTuple();
                if (!right_->is_end()) {
                    auto r2 = right_->Next();
                    if (compare_keys(*left_record_, *r2) != 0) break;
                    right_position_++;
                    right_matches_.push_back({*r2, right_position_});
                }
            }
            break;
        }

        if (right_matches_.empty()) {
            if (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN) {
                zero_right_record();
                null_pad_output_ = true;
                is_end_ = false;
                return;
            }
            is_end_ = true;
            return;
        }

        for (size_t i = 0; i < right_matches_.size(); i++) {
            set_right_record(right_matches_[i].rec);
            if (check_all_conds()) {
                left_has_group_match_ = true;
                null_pad_output_ = false;
                right_match_pos_ = i + 1;
                mark_right_match(right_matches_[i].pos);
                is_end_ = false;
                return;
            }
        }

        if (!advance_to_next_match()) {
            is_end_ = true;
            return;
        }
        is_end_ = false;
    }

    void start_unmatched_right_phase() {
        left_exhausted_ = true;
        memset(null_left_.data, 0, null_left_.size);
        right_->beginTuple();
        right_row_count_ = 0;
        if (find_unmatched_right()) {
            is_end_ = false;
        } else {
            is_end_ = true;
        }
    }

    void nextTuple() override {
        if (left_exhausted_) {
            if (!find_unmatched_right()) {
                is_end_ = true;
            }
            return;
        }

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
            left_has_group_match_ = false;
            null_pad_output_ = false;

            right_->beginTuple();
            right_matches_.clear();
            right_match_pos_ = 0;
            right_position_ = 0;

            while (!right_->is_end()) {
                auto rec = right_->Next();
                int cmp = compare_keys(*left_record_, *rec);
                if (cmp < 0) break;
                if (cmp == 0) {
                    right_position_++;
                    right_matches_.push_back({*rec, right_position_});
                }
                right_->nextTuple();
            }

            if (right_matches_.empty()) {
                if (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN) {
                    null_pad_output_ = true;
                    return;
                }
                do {
                    left_->nextTuple();
                    if (left_->is_end()) { is_end_ = true; return; }
                    left_record_ = left_->Next();
                    right_->beginTuple();
                    right_matches_.clear();
                    right_position_ = 0;
                    while (!right_->is_end()) {
                        auto rec = right_->Next();
                        int cmp = compare_keys(*left_record_, *rec);
                        if (cmp < 0) break;
                        if (cmp == 0) {
                            right_position_++;
                            right_matches_.push_back({*rec, right_position_});
                        }
                        right_->nextTuple();
                    }
                } while (right_matches_.empty());
            }

            for (size_t j = 0; j < right_matches_.size(); j++) {
                set_right_record(right_matches_[j].rec);
                if (check_all_conds()) {
                    left_has_group_match_ = true;
                    null_pad_output_ = false;
                    right_match_pos_ = j + 1;
                    mark_right_match(right_matches_[j].pos);
                    return;
                }
            }
            if (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN) {
                null_pad_output_ = true;
                return;
            }
        }

        while (right_match_pos_ < right_matches_.size()) {
            set_right_record(right_matches_[right_match_pos_].rec);
            size_t matched_pos = right_matches_[right_match_pos_].pos;
            right_match_pos_++;
            if (check_all_conds()) {
                null_pad_output_ = false;
                mark_right_match(matched_pos);
                return;
            }
        }

        if (!advance_to_next_match()) {
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
        if (left_exhausted_) {
            memset(output_record_.data, 0, left_->tupleLen());
            memcpy(output_record_.data + left_->tupleLen(),
                   right_record_.data, right_->tupleLen());
        } else if (null_pad_output_) {
            memcpy(output_record_.data, left_record_->data, left_->tupleLen());
            memset(output_record_.data + left_->tupleLen(), 0, right_->tupleLen());
        } else {
            memcpy(output_record_.data, left_record_->data, left_->tupleLen());
            memcpy(output_record_.data + left_->tupleLen(),
                   right_record_.data, right_->tupleLen());
        }
        // Return a non-owning view into the pre-allocated output_record_ buffer.
        // Safe because callers process and discard results before the next Next() call.
        auto rec = std::make_unique<RmRecord>();
        rec->size = static_cast<int>(len_);
        rec->data = output_record_.data;
        rec->allocated_ = false;
        return rec;
    }

    Rid &rid() override { return _abstract_rid; }
};