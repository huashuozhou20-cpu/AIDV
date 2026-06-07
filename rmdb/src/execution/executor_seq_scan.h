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

class SeqScanExecutor : public AbstractExecutor {
   private:
    std::string tab_name_;              // 表的名称
    std::vector<Condition> conds_;      // scan的条件
    RmFileHandle *fh_;                  // 表的数据文件句柄
    std::vector<ColMeta> cols_;         // scan后生成的记录的字段
    size_t len_;                        // scan后生成的每条记录的长度
    std::vector<Condition> fed_conds_;  // 同conds_，两个字段相同

    Rid rid_;
    std::unique_ptr<RecScan> scan_;     // table_iterator
    bool is_end_;

    // Gap lock state for phantom prevention during FOR UPDATE range scans
    bool has_match_;       // at least one matching record has been returned
    bool guard_locked_;    // GAP lock acquired on the first non-matching record after match
    bool has_index_;       // whether the table has at least one index for gap locking
    int index_key_len_;    // total length of the first index key
    std::vector<std::pair<int,int>> idx_col_offsets_;  // (offset, len) for key extraction

    SmManager *sm_manager_;

    // 评估单条条件是否满足（支持递归 OR/NOT）
    bool eval_cond(const Condition &cond, const RmRecord &rec) {
        // Internal node: OR
        if (cond.op == OP_OR) {
            for (auto &child : cond.children) {
                if (eval_cond(child, rec)) return true;
            }
            return cond.children.empty();
        }
        // Internal node: NOT
        if (cond.op == OP_NOT) {
            for (auto &child : cond.children) {
                if (eval_cond(child, rec)) return false;
            }
            return true;
        }

        // Leaf: find lhs column
        const ColMeta *lhs_meta = nullptr;
        for (auto &col : cols_) {
            if (col.tab_name == cond.lhs_col.tab_name && col.name == cond.lhs_col.col_name) {
                lhs_meta = &col;
                break;
            }
        }
        if (!lhs_meta) return true;

        char *lhs_data = rec.data + lhs_meta->offset;

        // IS NULL / IS NOT NULL
        if (cond.op == OP_IS_NULL) {
            return check_is_null(lhs_data, lhs_meta->type, lhs_meta->len);
        }
        if (cond.op == OP_IS_NOT_NULL) {
            return !check_is_null(lhs_data, lhs_meta->type, lhs_meta->len);
        }

        // LIKE / NOT LIKE
        if (cond.op == OP_LIKE || cond.op == OP_NOT_LIKE) {
            std::string col_str(lhs_data, lhs_meta->len);
            col_str = col_str.c_str();  // trim null padding
            bool matched = like_match(col_str, cond.rhs_val.str_val);
            return (cond.op == OP_LIKE) ? matched : !matched;
        }

        // BETWEEN / NOT BETWEEN
        if (cond.op == OP_BETWEEN || cond.op == OP_NOT_BETWEEN) {
            int cmp_low = 0, cmp_high = 0;
            switch (lhs_meta->type) {
                case TYPE_INT: {
                    int a = *(int *)lhs_data;
                    cmp_low = (a < cond.rhs_val.int_val) ? -1 : ((a > cond.rhs_val.int_val) ? 1 : 0);
                    cmp_high = (a < cond.rhs_val2.int_val) ? -1 : ((a > cond.rhs_val2.int_val) ? 1 : 0);
                    break;
                }
                case TYPE_FLOAT: {
                    float a = *(float *)lhs_data;
                    cmp_low = (a < cond.rhs_val.float_val) ? -1 : ((a > cond.rhs_val.float_val) ? 1 : 0);
                    cmp_high = (a < cond.rhs_val2.float_val) ? -1 : ((a > cond.rhs_val2.float_val) ? 1 : 0);
                    break;
                }
                case TYPE_STRING:
                    cmp_low = memcmp(lhs_data, cond.rhs_val.raw->data, lhs_meta->len);
                    cmp_high = memcmp(lhs_data, cond.rhs_val2.raw->data, lhs_meta->len);
                    break;
            }
            bool in_range = (cmp_low >= 0 && cmp_high <= 0);
            return (cond.op == OP_BETWEEN) ? in_range : !in_range;
        }

        // IN / NOT IN
        if (cond.op == OP_IN || cond.op == OP_NOT_IN) {
            bool found = false;
            for (auto &v : cond.in_values) {
                int cmp = 0;
                switch (lhs_meta->type) {
                    case TYPE_INT: {
                        int a = *(int *)lhs_data;
                        cmp = (a < v.int_val) ? -1 : ((a > v.int_val) ? 1 : 0);
                        break;
                    }
                    case TYPE_FLOAT: {
                        float a = *(float *)lhs_data;
                        cmp = (a < v.float_val) ? -1 : ((a > v.float_val) ? 1 : 0);
                        break;
                    }
                    case TYPE_STRING:
                        cmp = memcmp(lhs_data, v.raw->data, lhs_meta->len);
                        break;
                }
                if (cmp == 0) { found = true; break; }
            }
            return (cond.op == OP_IN) ? found : !found;
        }

        // Regular comparison (col op value)
        if (cond.is_rhs_val) {
            int cmp = 0;
            switch (lhs_meta->type) {
                case TYPE_INT: {
                    int a = *(int *)lhs_data, b = cond.rhs_val.int_val;
                    cmp = (a < b) ? -1 : ((a > b) ? 1 : 0);
                    break;
                }
                case TYPE_FLOAT: {
                    float a = *(float *)lhs_data, b = cond.rhs_val.float_val;
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
        } else {
            // col1 op col2
            const ColMeta *rhs_meta = nullptr;
            for (auto &col : cols_) {
                if (col.tab_name == cond.rhs_col.tab_name && col.name == cond.rhs_col.col_name) {
                    rhs_meta = &col;
                    break;
                }
            }
            if (!rhs_meta) return true;

            char *rhs_data = rec.data + rhs_meta->offset;
            int cmp = 0;
            switch (lhs_meta->type) {
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
                    cmp = memcmp(lhs_data, rhs_data, lhs_meta->len);
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
    }


    // 检查当前记录是否满足所有条件（AND 语义）
    bool check_all_conds(const RmRecord &rec) {
        for (auto &cond : fed_conds_) {
            if (!eval_cond(cond, rec)) return false;
        }
        return true;
    }

   public:
    SeqScanExecutor(SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, Context *context) {
        sm_manager_ = sm_manager;
        tab_name_ = std::move(tab_name);
        conds_ = std::move(conds);
        TabMeta &tab = sm_manager_->db_.get_table(tab_name_);
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        cols_ = tab.cols;
        len_ = cols_.back().offset + cols_.back().len;
        // Pre-compute index metadata for gap key extraction
        has_index_ = !tab.indexes.empty();
        if (has_index_) {
            auto &idx = tab.indexes[0];
            index_key_len_ = idx.col_tot_len;
            for (auto &idx_col : idx.cols) {
                // Find the matching table column to get its offset
                for (auto &col : tab.cols) {
                    if (col.name == idx_col.name) {
                        idx_col_offsets_.push_back({col.offset, col.len});
                        break;
                    }
                }
            }
        } else {
            index_key_len_ = 0;
        }

        context_ = context;
        fed_conds_ = conds_;
        is_end_ = true;
        has_match_ = false;
        guard_locked_ = false;
    }

    // Returns a visible version of the record at the given rid, following the
    // MVCC version chain through undo logs if the current version is not visible.
    std::unique_ptr<RmRecord> get_visible_record(const Rid& rid) {
        auto rec = fh_->get_record_snapshot(rid);
        if (!rec) return nullptr;

        txn_id_t trx_id;
        memcpy(&trx_id, rec->data + trx_id_offset(rec->size), sizeof(txn_id_t));

        if (context_->txn_->is_visible(trx_id)) {
            return rec;
        }

        // Follow version chain through undo logs
        UndoLink roll_ptr;
        memcpy(&roll_ptr, rec->data + roll_ptr_offset(rec->size), sizeof(UndoLink));

        int chain_len = 0;
        while (roll_ptr.IsValid()) {
            // Safety valve: abandon if chain exceeds extreme depth
            // (e.g. 8 writers × 45s pinned watermark × ~500 tps ≈ 180k updates).
            if (++chain_len > 100000) return nullptr;

            auto undo_opt = context_->txn_mgr_->GetUndoLogOptional(roll_ptr);
            if (!undo_opt.has_value()) return nullptr;

            auto& undo = *undo_opt;
            if (undo.is_deleted_) return nullptr;
            if (undo.old_data_.empty()) return nullptr;

            // Read trx_id from the old version's hidden fields
            int old_size = static_cast<int>(undo.old_data_.size());
            // Defensive: old record must be large enough to hold hidden fields.
            if (old_size < RM_HIDDEN_TOTAL_SIZE) return nullptr;

            const char* old_ptr = undo.old_data_.data();
            txn_id_t old_trx_id;
            memcpy(&old_trx_id, old_ptr + trx_id_offset(old_size), sizeof(txn_id_t));

            if (context_->txn_->is_visible(old_trx_id)) {
                // Deep-copy the old version data. GetUndoLogOptional returns
                // by value, so old_ptr points into a local copy that dies
                // when this function returns. We must allocate our own buffer.
                return std::make_unique<RmRecord>(old_size, const_cast<char*>(old_ptr));
            }

            roll_ptr = undo.prev_version_;
        }

        return nullptr;  // no visible version in chain
    }

    // Check whether we should use MVCC lock-free snapshot reads.
    bool use_mvcc_read() const {
        return context_->txn_mgr_ != nullptr
            && context_->txn_ != nullptr
            && context_->txn_->has_read_view()
            && context_->txn_->is_read_only();
    }

    bool use_for_update() const { return context_->is_for_update_; }

    void get_index_key(const RmRecord &rec, std::vector<char> &key_buf, int &key_len) {
        if (!has_index_) { key_len = 0; return; }
        key_buf.resize(index_key_len_);
        int pos = 0;
        for (auto &oc : idx_col_offsets_) {
            memcpy(key_buf.data() + pos, rec.data + oc.first, oc.second);
            pos += oc.second;
        }
        key_len = index_key_len_;
    }

    void beginTuple() override {
        has_match_ = false;
        guard_locked_ = false;
        scan_ = std::make_unique<RmScan>(fh_);
        if (scan_->is_end()) {
            is_end_ = true;
            return;
        }
        is_end_ = false;
        rid_ = scan_->rid();
        if (use_for_update()) {
            auto rec = fh_->get_record_for_update(rid_, context_);
            // Lock-before-filter: every index-touched record gets a GAP lock
            // regardless of non-indexed predicate filtering (InnoDB behavior).
            std::vector<char> idx_key; int key_len = 0;
            get_index_key(*rec, idx_key, key_len);
            if (key_len > 0) {
                context_->lock_mgr_->lock_gap_on_key(context_->txn_, fh_->GetFd(),
                    0, idx_key.data(), key_len);
            } else {
                context_->lock_mgr_->lock_gap(context_->txn_, rid_, fh_->GetFd());
            }
            has_match_ = true;
            if (!check_all_conds(*rec)) {
                nextTuple();
            }
        } else if (use_mvcc_read()) {
            auto rec = get_visible_record(rid_);
            if (!rec || !check_all_conds(*rec)) {
                nextTuple();
            }
        } else {
            char *snap_buf = context_->arena_.Allocate(len_);
            if (snap_buf) {
                if (!fh_->get_record_into(rid_, snap_buf) ||
                    !check_all_conds(RmRecord(len_, snap_buf, false))) {
                    nextTuple();
                }
            } else {
                auto snap = fh_->get_record_snapshot(rid_);
                if (!snap || !check_all_conds(*snap)) {
                    nextTuple();
                }
            }
        }
    }

    void nextTuple() override {
        bool for_update = use_for_update();
        bool mvcc = !for_update && use_mvcc_read();

        // If guard was already locked, stop scan
        if (guard_locked_) {
            is_end_ = true;
            return;
        }

        while (true) {
            scan_->next();
            if (scan_->is_end()) {
                is_end_ = true;
                return;
            }
            rid_ = scan_->rid();
            if (for_update) {
                auto rec = fh_->get_record_for_update(rid_, context_);
                // Lock-before-filter: GAP lock on every index-touched record.
                std::vector<char> idx_key; int key_len = 0;
                get_index_key(*rec, idx_key, key_len);
                if (key_len > 0) {
                    context_->lock_mgr_->lock_gap_on_key(context_->txn_, fh_->GetFd(),
                        0, idx_key.data(), key_len);
                } else {
                    context_->lock_mgr_->lock_gap(context_->txn_, rid_, fh_->GetFd());
                }
                if (check_all_conds(*rec)) {
                    has_match_ = true;
                    return;
                }
                if (has_match_) {
                    // First non-match after match (or after first locked record):
                    // guard GAP already acquired above; stop scan.
                    guard_locked_ = true;
                    is_end_ = true;
                    return;
                }
                has_match_ = true;
                // Record locked but predicate-filtered — continue scanning
            } else if (mvcc) {
                auto rec = get_visible_record(rid_);
                if (rec && check_all_conds(*rec)) return;
            } else {
                // Arena-backed snapshot: skip heap allocation
                char *snap_buf = context_->arena_.Allocate(len_);
                if (snap_buf) {
                    if (!fh_->get_record_into(rid_, snap_buf)) continue;
                    RmRecord snap(len_, snap_buf, false);
                    if (!check_all_conds(snap)) continue;
                } else {
                    auto snap = fh_->get_record_snapshot(rid_);
                    if (!snap || !check_all_conds(*snap)) continue;
                }
                auto rec = fh_->get_record(rid_, context_);
                if (check_all_conds(*rec)) return;
            }
        }
    }

    bool is_end() const override { return is_end_; }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    ColMeta get_col_offset(const TabCol &target) override {
        for (auto &col : cols_) {
            if (col.name == target.col_name &&
                (target.tab_name.empty() || col.tab_name == target.tab_name))
                return col;
        }
        return ColMeta{};
    }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end_) return nullptr;
        if (use_for_update()) {
            return fh_->get_record(rid_, context_);
        }
        if (use_mvcc_read()) {
            return get_visible_record(rid_);
        }
        // Arena-backed: avoid heap alloc for the final record fetch
        char *buf = context_->arena_.Allocate(len_);
        if (buf && fh_->get_record_into(rid_, buf)) {
            return std::make_unique<RmRecord>(static_cast<int>(len_), buf, false);
        }
        return fh_->get_record(rid_, context_);
    }

    Rid &rid() override { return rid_; }
};