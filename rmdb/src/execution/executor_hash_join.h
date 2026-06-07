#pragma once

#include <unordered_map>
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class HashJoinExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> left_;   // build side (small table)
    std::unique_ptr<AbstractExecutor> right_;  // probe side (large table)
    size_t len_;
    std::vector<ColMeta> cols_;
    JoinType join_type_;
    std::vector<Condition> conds_;
    std::vector<Condition> equi_conds_;
    bool is_end_;

    // Build side
    std::vector<RmRecord> build_records_;
    std::vector<bool> build_matched_;
    std::unordered_multimap<std::string, size_t> hash_table_;  // key -> index into build_records_

    // Probe state
    std::unique_ptr<RmRecord> right_record_;
    size_t right_row_count_;
    // Current range scan state
    std::pair<decltype(hash_table_)::const_iterator,
              decltype(hash_table_)::const_iterator> cur_range_;
    size_t cur_right_row_;     // which right row produced cur_range_
    size_t cur_match_build_;   // most recently matched build_records_ index
    bool right_exhausted_;
    std::vector<bool> right_matched_;  // for FULL JOIN unmatched-right phase

    // Outer-join phases
    enum Phase { PHASE_PROBE, PHASE_NULL_RIGHT, PHASE_NULL_LEFT };
    Phase phase_;
    size_t null_emit_idx_;  // current index for null-padding phases

    // Pre-computed: for each equi-cond, whether lhs_col belongs to left executor
    std::vector<bool> equi_lhs_is_left_;

    // Pre-allocated buffers
    RmRecord output_record_;
    RmRecord null_right_;

    // Lookup column metadata in a specific executor's cols (uses original offsets)
    ColMeta get_col_meta_in(const TabCol &target,
                            const std::vector<ColMeta> &search_cols) {
        for (auto &col : search_cols) {
            if (col.tab_name == target.tab_name && col.name == target.col_name)
                return col;
        }
        return ColMeta{};
    }

    ColMeta get_col_meta(const TabCol &target) {
        for (auto &col : cols_) {
            if (col.tab_name == target.tab_name && col.name == target.col_name) return col;
        }
        for (auto &col : left_->cols()) {
            if (col.tab_name == target.tab_name && col.name == target.col_name) return col;
        }
        for (auto &col : right_->cols()) {
            if (col.tab_name == target.tab_name && col.name == target.col_name) return col;
        }
        return ColMeta{};
    }

    std::string build_key(const RmRecord &rec, bool is_left) {
        std::string key;
        const auto &search_cols = is_left ? left_->cols() : right_->cols();
        for (size_t i = 0; i < equi_conds_.size(); i++) {
            auto &cond = equi_conds_[i];
            const TabCol &col = equi_lhs_is_left_[i] == is_left ? cond.lhs_col : cond.rhs_col;
            ColMeta meta = get_col_meta_in(col, search_cols);
            char *data = rec.data + meta.offset;
            switch (meta.type) {
                case TYPE_INT:
                    key.append((char *)data, sizeof(int));
                    break;
                case TYPE_FLOAT:
                    key.append((char *)data, sizeof(float));
                    break;
                case TYPE_STRING:
                    key.append(data, strnlen(data, meta.len));
                    break;
            }
            key += '\0';
        }
        return key;
    }

    // Get column offset from a specific executor's cols (original offsets)
    int get_offset_in(const TabCol &target, const std::vector<ColMeta> &search_cols) {
        for (auto &col : search_cols) {
            if (col.tab_name == target.tab_name && col.name == target.col_name)
                return col.offset;
        }
        return -1;
    }

    int get_real_offset(const TabCol &target) {
        // Search left cols first (original offsets), then right cols
        int off = get_offset_in(target, left_->cols());
        if (off >= 0) return off;
        off = get_offset_in(target, right_->cols());
        if (off >= 0) return off;
        // Fallback: search combined cols
        for (auto &col : cols_) {
            if (col.tab_name == target.tab_name && col.name == target.col_name)
                return col.offset;
        }
        return -1;
    }

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

        ColMeta lhs_meta = get_col_meta(cond.lhs_col);
        int lhs_offset = get_real_offset(cond.lhs_col);
        bool lhs_is_left = lhs_meta.tab_name == left_->cols()[0].tab_name;
        char *lhs_data = nullptr;
        if (!lhs_meta.tab_name.empty()) {
            lhs_data = lhs_is_left ? left_rec.data + lhs_offset : right_rec.data + lhs_offset;
        }

        if (cond.op == OP_IS_NULL)
            return check_is_null(lhs_data, lhs_meta.type, lhs_meta.len);
        if (cond.op == OP_IS_NOT_NULL)
            return !check_is_null(lhs_data, lhs_meta.type, lhs_meta.len);

        ColMeta rhs_meta = get_col_meta(cond.rhs_col);
        int rhs_offset = get_real_offset(cond.rhs_col);
        bool rhs_is_left = rhs_meta.tab_name == left_->cols()[0].tab_name;
        char *rhs_data = nullptr;
        if (!rhs_meta.tab_name.empty()) {
            rhs_data = rhs_is_left ? left_rec.data + rhs_offset : right_rec.data + rhs_offset;
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

    bool check_all_conds(const RmRecord &left_rec, const RmRecord &right_rec) {
        if (conds_.empty()) return true;
        for (auto &cond : conds_) {
            if (!eval_cond(cond, left_rec, right_rec)) {
                return false;
            }
        }
        return true;
    }

    bool try_probe_next() {
        while (cur_range_.first != cur_range_.second) {
            size_t bidx = cur_range_.first->second;
            ++cur_range_.first;
            if (check_all_conds(build_records_[bidx], *right_record_)) {
                build_matched_[bidx] = true;
                cur_match_build_ = bidx;
                if (join_type_ == FULL_JOIN) {
                    if (cur_right_row_ >= right_matched_.size())
                        right_matched_.resize(cur_right_row_ + 1, false);
                    right_matched_[cur_right_row_] = true;
                }
                return true;
            }
        }
        return false;
    }

    bool advance_right() {
        while (true) {
            right_->nextTuple();
            if (right_->is_end()) {
                right_exhausted_ = true;
                return false;
            }
            right_record_ = right_->Next();
            cur_right_row_ = ++right_row_count_;
            std::string key = build_key(*right_record_, false);
            cur_range_ = hash_table_.equal_range(key);
            if (try_probe_next()) return true;
        }
    }

    bool emit_null_right() {
        while (null_emit_idx_ < build_matched_.size()) {
            if (!build_matched_[null_emit_idx_]) {
                build_matched_[null_emit_idx_] = true;
                cur_match_build_ = null_emit_idx_;
                null_emit_idx_++;
                return true;
            }
            null_emit_idx_++;
        }
        return false;
    }

    bool emit_null_left() {
        while (true) {
            right_->nextTuple();
            if (right_->is_end()) return false;
            right_record_ = right_->Next();
            cur_right_row_ = ++right_row_count_;
            if (cur_right_row_ >= right_matched_.size() || !right_matched_[cur_right_row_]) {
                return true;
            }
        }
    }

   public:
    HashJoinExecutor(std::unique_ptr<AbstractExecutor> left, std::unique_ptr<AbstractExecutor> right,
                     std::vector<Condition> conds, std::vector<Condition> equi_conds,
                     JoinType join_type = INNER_JOIN)
        : join_type_(join_type), conds_(std::move(conds)), equi_conds_(std::move(equi_conds)),
          is_end_(true), right_row_count_(0), cur_right_row_(0), cur_match_build_(0),
          right_exhausted_(false), phase_(PHASE_PROBE), null_emit_idx_(0) {

        left_ = std::move(left);
        right_ = std::move(right);

        len_ = left_->tupleLen() + right_->tupleLen();
        cols_ = left_->cols();
        auto right_cols = right_->cols();
        for (auto &col : right_cols) {
            col.offset += left_->tupleLen();
        }
        cols_.insert(cols_.end(), right_cols.begin(), right_cols.end());

        output_record_ = RmRecord(static_cast<int>(len_));
        null_right_ = RmRecord(static_cast<int>(right_->tupleLen()));
        memset(null_right_.data, 0, null_right_.size);

        for (auto &cond : equi_conds_) {
            equi_lhs_is_left_.push_back(cond.lhs_col.tab_name == left_->cols()[0].tab_name);
        }
    }

    void beginTuple() override {
        is_end_ = true;
        phase_ = PHASE_PROBE;
        right_exhausted_ = false;
        right_row_count_ = 0;
        cur_match_build_ = 0;
        null_emit_idx_ = 0;
        hash_table_.clear();
        build_records_.clear();
        build_matched_.clear();
        right_matched_.clear();

        // === Build: materialize entire left side ===
        left_->beginTuple();
        while (!left_->is_end()) {
            auto rec = left_->Next();
            size_t idx = build_records_.size();
            build_records_.push_back(RmRecord(rec->size));
            memcpy(build_records_[idx].data, rec->data, rec->size);
            build_records_[idx].size = rec->size;
            build_matched_.push_back(false);

            std::string key = build_key(build_records_[idx], true);
            hash_table_.emplace(std::move(key), idx);

            left_->nextTuple();
        }

        if (build_records_.empty()) {
            if (join_type_ == FULL_JOIN) {
                phase_ = PHASE_NULL_LEFT;
                right_->beginTuple();
                right_row_count_ = 0;
                is_end_ = !emit_null_left();
            }
            return;
        }

        // === Probe: scan right side ===
        right_->beginTuple();
        right_row_count_ = 0;
        if (!right_->is_end()) {
            right_record_ = right_->Next();
            cur_right_row_ = ++right_row_count_;
            std::string key = build_key(*right_record_, false);
            cur_range_ = hash_table_.equal_range(key);
            if (try_probe_next()) {
                is_end_ = false;
                return;
            }
        } else {
            right_exhausted_ = true;
        }

        if (advance_right()) {
            is_end_ = false;
            return;
        }

        if (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN) {
            phase_ = PHASE_NULL_RIGHT;
            null_emit_idx_ = 0;
            if (emit_null_right()) {
                is_end_ = false;
                return;
            }
            if (join_type_ == FULL_JOIN) {
                phase_ = PHASE_NULL_LEFT;
                right_->beginTuple();
                right_row_count_ = 0;
                is_end_ = !emit_null_left();
            }
        }
    }

    void nextTuple() override {
        switch (phase_) {
            case PHASE_PROBE: {
                if (!right_exhausted_ && try_probe_next()) return;
                if (advance_right()) return;

                if (join_type_ == LEFT_JOIN || join_type_ == FULL_JOIN) {
                    phase_ = PHASE_NULL_RIGHT;
                    null_emit_idx_ = 0;
                    if (emit_null_right()) return;
                }
                if (join_type_ == FULL_JOIN) {
                    phase_ = PHASE_NULL_LEFT;
                    right_->beginTuple();
                    right_row_count_ = 0;
                    if (emit_null_left()) return;
                }
                is_end_ = true;
                break;
            }
            case PHASE_NULL_RIGHT: {
                if (emit_null_right()) return;
                if (join_type_ == FULL_JOIN) {
                    phase_ = PHASE_NULL_LEFT;
                    right_->beginTuple();
                    right_row_count_ = 0;
                    if (emit_null_left()) return;
                }
                is_end_ = true;
                break;
            }
            case PHASE_NULL_LEFT: {
                if (!emit_null_left()) is_end_ = true;
                break;
            }
        }
    }

    bool is_end() const override { return is_end_; }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end_) return nullptr;

        switch (phase_) {
            case PHASE_PROBE:
            case PHASE_NULL_RIGHT: {
                // matched left + (possibly NULL) right
                memcpy(output_record_.data, build_records_[cur_match_build_].data, left_->tupleLen());
                if (phase_ == PHASE_NULL_RIGHT) {
                    memset(output_record_.data + left_->tupleLen(), 0, right_->tupleLen());
                } else {
                    memcpy(output_record_.data + left_->tupleLen(), right_record_->data, right_->tupleLen());
                }
                break;
            }
            case PHASE_NULL_LEFT: {
                memset(output_record_.data, 0, left_->tupleLen());
                memcpy(output_record_.data + left_->tupleLen(), right_record_->data, right_->tupleLen());
                break;
            }
        }

        auto rec = std::make_unique<RmRecord>();
        rec->size = static_cast<int>(len_);
        rec->data = output_record_.data;
        rec->allocated_ = false;
        return rec;
    }

    Rid &rid() override { return _abstract_rid; }
};
