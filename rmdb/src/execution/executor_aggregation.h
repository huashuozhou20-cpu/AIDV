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

#include <unordered_map>
#include <unordered_set>
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "system/sm.h"

class AggregationExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> child_;
    std::vector<ColMeta> cols_;
    size_t len_;
    bool is_end_;
    std::vector<std::unique_ptr<RmRecord>> results_;
    size_t result_pos_;

    std::vector<ast::AggType> agg_types_;
    std::vector<std::string> agg_cols_;
    std::vector<std::string> group_by_cols_;
    std::vector<Condition> having_conds_;

    struct AggregateState {
        int count = 0;
        double sum = 0.0;
        double max_val = 0.0;
        double min_val = 0.0;
        bool has_value = false;
    };

    struct GroupKey {
        std::vector<char> data;
        size_t total_len;
        bool operator==(const GroupKey &other) const {
            return total_len == other.total_len && memcmp(data.data(), other.data.data(), total_len) == 0;
        }
    };
    struct GroupKeyHash {
        size_t operator()(const GroupKey &k) const {
            size_t h = 0;
            for (size_t i = 0; i < k.total_len && i < 32; i++)
                h = h * 31 + k.data[i];
            return h;
        }
    };

    ColMeta find_col(const std::string &col_name) {
        for (auto &col : child_->cols()) {
            if (col.name == col_name || col.tab_name + "." + col.name == col_name) return col;
        }
        // If col_name has table prefix, strip it and try again
        auto dot_pos = col_name.find('.');
        std::string bare_name = (dot_pos != std::string::npos) ? col_name.substr(dot_pos + 1) : col_name;
        for (auto &col : child_->cols()) {
            if (col.name == bare_name) return col;
        }
        return ColMeta{};
    }

    Value compute_value(AggregateState &state, ast::AggType type) {
        Value v;
        switch (type) {
            case ast::AGG_COUNT:
                v.set_int(state.count);
                v.init_raw(sizeof(int));
                break;
            case ast::AGG_SUM:
                v.set_float(static_cast<float>(state.sum));
                v.init_raw(sizeof(float));
                break;
            case ast::AGG_AVG:
                if (state.count > 0)
                    v.set_float(static_cast<float>(state.sum / state.count));
                else
                    v.set_float(0.0f);
                v.init_raw(sizeof(float));
                break;
            case ast::AGG_MAX:
                if (state.has_value)
                    v.set_float(static_cast<float>(state.max_val));
                else
                    v.set_float(0.0f);
                v.init_raw(sizeof(float));
                break;
            case ast::AGG_MIN:
                if (state.has_value)
                    v.set_float(static_cast<float>(state.min_val));
                else
                    v.set_float(0.0f);
                v.init_raw(sizeof(float));
                break;
        }
        return v;
    }

    static std::string agg_type_name(ast::AggType t) {
        switch (t) {
            case ast::AGG_COUNT: return "COUNT";
            case ast::AGG_SUM:   return "SUM";
            case ast::AGG_AVG:   return "AVG";
            case ast::AGG_MAX:   return "MAX";
            case ast::AGG_MIN:   return "MIN";
        }
        return "UNKNOWN";
    }

    void accumulate(AggregateState &state, const char *data, ColType type) {
        state.count++;
        double val = 0.0;
        switch (type) {
            case TYPE_INT: val = static_cast<double>(*(const int *)data); break;
            case TYPE_FLOAT: val = static_cast<double>(*(const float *)data); break;
            case TYPE_STRING: val = 0.0; break;
        }
        state.sum += val;
        if (!state.has_value) {
            state.max_val = val;
            state.min_val = val;
            state.has_value = true;
        } else {
            if (val > state.max_val) state.max_val = val;
            if (val < state.min_val) state.min_val = val;
        }
    }

    // Evaluate HAVING condition against aggregate result record
    bool eval_having_cond(const Condition &cond, const RmRecord &rec) {
        // Find the aggregate column in our output cols
        const ColMeta *lhs_meta = nullptr;
        for (auto &col : cols_) {
            if (col.name == cond.lhs_col.col_name) {
                lhs_meta = &col;
                break;
            }
        }
        if (!lhs_meta) return true;

        char *lhs_data = rec.data + lhs_meta->offset;
        if (cond.op == OP_IS_NULL)
            return check_is_null(lhs_data, lhs_meta->type, lhs_meta->len);
        if (cond.op == OP_IS_NOT_NULL)
            return !check_is_null(lhs_data, lhs_meta->type, lhs_meta->len);

        if (!cond.is_rhs_val) return true;

        int cmp = 0;
        switch (lhs_meta->type) {
            case TYPE_INT: {
                int a = *(int *)lhs_data, b = cond.rhs_val.int_val;
                if (cond.rhs_val.type == TYPE_FLOAT)
                    b = (int)cond.rhs_val.float_val;
                cmp = (a < b) ? -1 : ((a > b) ? 1 : 0);
                break;
            }
            case TYPE_FLOAT: {
                float a = *(float *)lhs_data, b;
                if (cond.rhs_val.type == TYPE_INT)
                    b = (float)cond.rhs_val.int_val;
                else
                    b = cond.rhs_val.float_val;
                cmp = (a < b) ? -1 : ((a > b) ? 1 : 0);
                break;
            }
            case TYPE_STRING:
                cmp = memcmp(lhs_data, cond.rhs_val.raw->data, lhs_meta->len);
                break;
        }
        switch (cond.op) {
            case OP_EQ: return cmp == 0;
            case OP_NE: return cmp != 0;
            case OP_LT: return cmp < 0;
            case OP_GT: return cmp > 0;
            case OP_LE: return cmp <= 0;
            case OP_GE: return cmp >= 0;
            default: return true;
        }
    }

    bool check_having(const RmRecord &rec) {
        for (auto &cond : having_conds_) {
            if (!eval_having_cond(cond, rec)) return false;
        }
        return true;
    }

   public:
    AggregationExecutor(std::unique_ptr<AbstractExecutor> child,
                        std::vector<ast::AggType> agg_types,
                        std::vector<std::string> agg_cols,
                        std::vector<std::string> group_by_cols,
                        std::vector<Condition> having_conds)
        : child_(std::move(child)), agg_types_(std::move(agg_types)),
          agg_cols_(std::move(agg_cols)), group_by_cols_(std::move(group_by_cols)),
          having_conds_(std::move(having_conds)) {

        // Build output column metadata
        int offset = 0;
        for (auto &gb_col : group_by_cols_) {
            ColMeta gb_meta = find_col(gb_col);
            gb_meta.offset = offset;
            offset += gb_meta.len;
            cols_.push_back(gb_meta);
        }
        for (size_t i = 0; i < agg_types_.size(); i++) {
            ColMeta meta;
            meta.tab_name = "";
            meta.name = agg_cols_[i];
            meta.len = (agg_types_[i] == ast::AGG_COUNT) ? sizeof(int) : sizeof(float);
            meta.type = (agg_types_[i] == ast::AGG_COUNT) ? TYPE_INT : TYPE_FLOAT;
            meta.offset = offset;
            offset += meta.len;
            cols_.push_back(meta);
        }
        len_ = offset;
        result_pos_ = 0;
        is_end_ = true;
    }

    void beginTuple() override {
        results_.clear();
        result_pos_ = 0;

        if (group_by_cols_.empty()) {
            // No GROUP BY: single aggregate row
            std::vector<AggregateState> states(agg_types_.size());

            child_->beginTuple();
            while (!child_->is_end()) {
                auto rec = child_->Next();
                for (size_t i = 0; i < agg_types_.size(); i++) {
                    if (agg_cols_[i].empty()) {
                        // COUNT(*) or similar: just count, no data access needed
                        states[i].count++;
                        states[i].has_value = true;
                    } else {
                        ColMeta agg_meta = find_col(agg_cols_[i]);
                        char *agg_data = rec->data + agg_meta.offset;
                        accumulate(states[i], agg_data, agg_meta.type);
                    }
                }
                child_->nextTuple();
            }

            auto rec = std::make_unique<RmRecord>(len_);
            memset(rec->data, 0, len_);
            for (size_t i = 0; i < agg_types_.size(); i++) {
                if (states[i].has_value || agg_types_[i] == ast::AGG_COUNT) {
                    Value v = compute_value(states[i], agg_types_[i]);
                    memcpy(rec->data + cols_[i].offset, v.raw->data, cols_[i].len);
                }
            }
            if (check_having(*rec)) {
                results_.push_back(std::move(rec));
            }
        } else {
            // GROUP BY
            std::unordered_map<GroupKey, std::vector<AggregateState>, GroupKeyHash> groups;

            size_t gb_len = 0;
            for (auto &gb_col : group_by_cols_) {
                ColMeta cm = find_col(gb_col);
                gb_len += cm.len;
            }

            child_->beginTuple();
            while (!child_->is_end()) {
                auto rec = child_->Next();

                // Build group key
                GroupKey key;
                key.total_len = gb_len;
                key.data.resize(gb_len);
                int off = 0;
                for (auto &gb_col : group_by_cols_) {
                    ColMeta cm = find_col(gb_col);
                    memcpy(key.data.data() + off, rec->data + cm.offset, cm.len);
                    off += cm.len;
                }

                auto &states = groups[key];
                if (states.empty()) {
                    states.resize(agg_types_.size());
                }

                for (size_t i = 0; i < agg_types_.size(); i++) {
                    ColMeta am = find_col(agg_cols_[i]);
                    char *agg_data = rec->data + am.offset;
                    accumulate(states[i], agg_data, am.type);
                }

                child_->nextTuple();
            }

            // Build result rows
            for (auto &[key, states] : groups) {
                auto rec = std::make_unique<RmRecord>(len_);
                // Copy group by columns
                int off = 0;
                for (size_t i = 0; i < group_by_cols_.size(); i++) {
                    ColMeta cm = find_col(group_by_cols_[i]);
                    memcpy(rec->data + cols_[i].offset, key.data.data() + off, cm.len);
                    off += cm.len;
                }
                // Compute and write aggregates
                for (size_t i = 0; i < agg_types_.size(); i++) {
                    Value v = compute_value(states[i], agg_types_[i]);
                    memcpy(rec->data + cols_[group_by_cols_.size() + i].offset,
                           v.raw->data, cols_[group_by_cols_.size() + i].len);
                }
                if (check_having(*rec)) {
                    results_.push_back(std::move(rec));
                }
            }
        }

        is_end_ = results_.empty();
    }

    void nextTuple() override {
        result_pos_++;
        if (result_pos_ >= results_.size()) {
            is_end_ = true;
        }
    }

    bool is_end() const override { return is_end_; }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end_ || result_pos_ >= results_.size()) return nullptr;
        auto rec = std::make_unique<RmRecord>(len_);
        memcpy(rec->data, results_[result_pos_]->data, len_);
        return rec;
    }

    Rid &rid() override { return _abstract_rid; }
};
