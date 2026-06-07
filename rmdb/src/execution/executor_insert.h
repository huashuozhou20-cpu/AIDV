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

class InsertExecutor : public AbstractExecutor {
   private:
    TabMeta tab_;                   // 表的元数据
    std::vector<std::vector<Value>> values_list_;  // 多行插入数据
    std::vector<std::string> col_names_;  // 可选的列名列表
    RmFileHandle *fh_;              // 表的数据文件句柄
    std::string tab_name_;          // 表名称
    Rid rid_;
    SmManager *sm_manager_;
    size_t current_row_ = 0;
    std::vector<int> auto_inc_next_;  // next AUTO_INCREMENT value per column (-1 = not auto_inc)

    // Build a mapping from column name to index in the table
    std::map<std::string, int> col_name_to_idx_;

   public:
    InsertExecutor(SmManager *sm_manager, const std::string &tab_name,
                   std::vector<std::vector<Value>> values_list,
                   std::vector<std::string> col_names, Context *context) {
        sm_manager_ = sm_manager;
        tab_ = sm_manager_->db_.get_table(tab_name);
        values_list_ = std::move(values_list);
        col_names_ = std::move(col_names);
        tab_name_ = tab_name;

        // Build column name → index map
        for (size_t i = 0; i < tab_.cols.size(); i++) {
            col_name_to_idx_[tab_.cols[i].name] = (int)i;
        }

        // Validate row sizes
        size_t expected = col_names_.empty() ? tab_.cols.size() : col_names_.size();
        for (auto &row : values_list_) {
            if (row.size() != expected) {
                throw InvalidValueCountError();
            }
        }
        fh_ = sm_manager_->fhs_.at(tab_name).get();
        context_ = context;

        // Find AUTO_INCREMENT columns and use cached next value
        auto_inc_next_.resize(tab_.cols.size(), -1);
        for (size_t ci = 0; ci < tab_.cols.size(); ci++) {
            if (tab_.cols[ci].auto_increment && tab_.cols[ci].type == TYPE_INT) {
                auto_inc_next_[ci] = tab_.next_auto_inc;
            }
        }
    };

    std::unique_ptr<RmRecord> Next() override {
        while (current_row_ < values_list_.size()) {
            auto &row_vals = values_list_[current_row_++];
            // Make record buffer (initially all zeros = NULL)
            RmRecord rec(fh_->get_file_hdr().record_size);
            memset(rec.data, 0, fh_->get_file_hdr().record_size);

            // Track which columns were explicitly set
            std::vector<bool> col_set(tab_.cols.size(), false);

            // Map values to table columns
            for (size_t vi = 0; vi < row_vals.size(); vi++) {
                auto &val = row_vals[vi];
                int col_idx;
                ColMeta *col;
                if (col_names_.empty()) {
                    col_idx = (int)vi;
                    col = &tab_.cols[vi];
                } else {
                    auto it = col_name_to_idx_.find(col_names_[vi]);
                    if (it == col_name_to_idx_.end()) {
                        throw RMDBError("Column '" + col_names_[vi] + "' not found in table '" + tab_name_ + "'");
                    }
                    col_idx = it->second;
                    col = &tab_.cols[col_idx];
                }
                col_set[col_idx] = true;
                // Allow INT->FLOAT implicit conversion
                if (col->type == TYPE_FLOAT && val.type == TYPE_INT && !val.is_null_) {
                    val.set_float((float)val.int_val);
                }
                if (col->type != val.type && !val.is_null_) {
                    throw IncompatibleTypeError(coltype2str(col->type), coltype2str(val.type));
                }
                if (col->not_null && val.is_null_) {
                    throw RMDBError("Column '" + col->name + "' cannot be NULL");
                }
                // AUTO_INCREMENT: replace NULL with next value
                if (val.is_null_ && auto_inc_next_[col_idx] > 0) {
                    val.set_int(auto_inc_next_[col_idx]++);
                    val.is_null_ = false;
                }
                val.init_raw(col->len);
                memcpy(rec.data + col->offset, val.raw->data, col->len);
            }
            // Fill AUTO_INCREMENT and DEFAULT values for unspecified columns
            for (size_t ci = 0; ci < tab_.cols.size(); ci++) {
                if (col_set[ci]) continue;
                if (auto_inc_next_[ci] > 0) {
                    int val = auto_inc_next_[ci]++;
                    memcpy(rec.data + tab_.cols[ci].offset, &val, sizeof(int));
                } else if (tab_.cols[ci].has_default) {
                    auto &d = tab_.cols[ci].default_val;
                    auto &col = tab_.cols[ci];
                    if (col.type == TYPE_INT) {
                        int v = d.empty() ? 0 : std::stoi(d);
                        memcpy(rec.data + col.offset, &v, sizeof(int));
                    } else if (col.type == TYPE_FLOAT) {
                        float v = d.empty() ? 0.0f : std::stof(d);
                        memcpy(rec.data + col.offset, &v, sizeof(float));
                    } else if (col.type == TYPE_STRING) {
                        memset(rec.data + col.offset, 0, col.len);
                        memcpy(rec.data + col.offset, d.c_str(), std::min(d.size(), (size_t)col.len));
                    }
                }
            }
            // Eager unique constraint check: probe the primary index BEFORE inserting.
            // If the key already exists, reject immediately.
            if (!tab_.indexes.empty()) {
                auto& pk_idx = tab_.indexes[0];
                std::vector<char> pk_key(pk_idx.col_tot_len);
                int off = 0;
                for (const auto& col : pk_idx.cols) {
                    memcpy(pk_key.data() + off, rec.data + col.offset, col.len);
                    off += col.len;
                }
                auto ih = sm_manager_->ihs_.at(
                    sm_manager_->get_ix_manager()->get_index_name(
                        tab_name_, pk_idx.cols)).get();
                // Use find_leaf_page + leaf_lookup with RAII latch guard
                bool found = false;
                {
                    auto result = ih->find_leaf_page(pk_key.data(),
                        Operation::FIND, nullptr, false);
                    auto leaf = std::move(result.first);
                    PageLatchGuard latch(leaf->get_page(), false, adopt_latch);
                    Rid* rid;
                    found = leaf->leaf_lookup(pk_key.data(), &rid);
                }
                if (found) {
                    throw RMDBError("Duplicate key error: primary key already exists");
                }
            }

            // Acquire INSERT_INTENTION on the NEXT key BEFORE inserting.
            // We use B+tree to find the next-greater index key, then place
            // INSERT_INTENTION on THAT key's LockDataId. This collides with
            // any GAP/NEXT_KEY lock held by a concurrent range scan on the
            // same upper-bound key, blocking phantom inserts.
            if (context_ != nullptr && context_->lock_mgr_ != nullptr
                && !tab_.indexes.empty()) {
                auto& first_idx = tab_.indexes[0];
                try {
                    // Build the insert key for this new record
                    std::vector<char> insert_key(first_idx.col_tot_len);
                    int off = 0;
                    for (const auto& col : first_idx.cols) {
                        memcpy(insert_key.data() + off, rec.data + col.offset, col.len);
                        off += col.len;
                    }
                    auto ih = sm_manager_->ihs_.at(
                        sm_manager_->get_ix_manager()->get_index_name(
                            tab_name_, first_idx.cols)).get();
                    // Find the leaf page and get the first key > insert_key
                    auto result = ih->find_leaf_page(insert_key.data(),
                        Operation::FIND, nullptr, false);
                    auto leaf_guard = std::move(result.first);
                    PageLatchGuard latch(leaf_guard->get_page(), false, adopt_latch);
                    int slot = leaf_guard->upper_bound(insert_key.data());
                    const char* next_key = nullptr;
                    if (slot < leaf_guard->get_size()) {
                        next_key = leaf_guard->get_key(slot);
                    }
                    // Release the read latch BEFORE acquiring LockManager locks
                    // (otherwise insert_entry would deadlock on write latch)
                    latch.release();
                    if (next_key != nullptr) {
                        context_->lock_mgr_->lock_insert_intention_on_key(
                            context_->txn_, fh_->GetFd(), 0,
                            next_key, first_idx.col_tot_len);
                    }
                } catch (...) {
                    // If B+tree lookup fails, skip gap locking.
                    // The record-level X-lock from insert_record still protects.
                }
            }

            // Insert into record file
            rid_ = fh_->insert_record(rec.data, context_);

            // WAL: log the insert (chain prev_lsn for undo traversal)
            auto* insert_log = new InsertLogRecord(context_->txn_->get_transaction_id(), rec, rid_, tab_name_);
            insert_log->prev_lsn_ = context_->txn_->get_prev_lsn();
            auto insert_lsn = context_->log_mgr_->add_log_to_buffer(insert_log);
            context_->txn_->set_prev_lsn(insert_lsn);

            // Record for rollback
            auto* wr = new WriteRecord(WType::INSERT_TUPLE, tab_name_, rid_);
            context_->txn_->append_write_record(wr);

            // Insert into index
            for (size_t i = 0; i < tab_.indexes.size(); ++i) {
                auto &index = tab_.indexes[i];
                auto ih = sm_manager_->ihs_.at(sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols)).get();
                char *key = new char[index.col_tot_len];
                int offset = 0;
                for (size_t j = 0; j < static_cast<size_t>(index.col_num); ++j) {
                    memcpy(key + offset, rec.data + index.cols[j].offset, index.cols[j].len);
                    offset += index.cols[j].len;
                }
                ih->insert_entry(key, rid_, context_->txn_);
                delete[] key;
            }
        }
        // Update cached AUTO_INCREMENT value
        for (size_t ci = 0; ci < tab_.cols.size(); ci++) {
            if (auto_inc_next_[ci] > 0) {
                tab_.next_auto_inc = auto_inc_next_[ci];
                // Write back to DbMeta so subsequent inserts see the updated counter
                sm_manager_->db_.get_table(tab_name_).next_auto_inc = tab_.next_auto_inc;
                break;
            }
        }
        return nullptr;
    }
    Rid &rid() override { return rid_; }
};