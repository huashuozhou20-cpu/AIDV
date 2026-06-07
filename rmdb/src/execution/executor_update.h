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

class UpdateExecutor : public AbstractExecutor {
   private:
    TabMeta tab_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<Rid> rids_;
    std::string tab_name_;
    std::vector<SetClause> set_clauses_;
    SmManager *sm_manager_;

   public:
    UpdateExecutor(SmManager *sm_manager, const std::string &tab_name, std::vector<SetClause> set_clauses,
                   std::vector<Condition> conds, std::vector<Rid> rids, Context *context) {
        sm_manager_ = sm_manager;
        tab_name_ = tab_name;
        set_clauses_ = set_clauses;
        tab_ = sm_manager_->db_.get_table(tab_name);
        fh_ = sm_manager_->fhs_.at(tab_name).get();
        conds_ = conds;
        rids_ = rids;
        context_ = context;
    }
    std::unique_ptr<RmRecord> Next() override {
        for (auto &rid : rids_) {
            // 读取旧记录
            auto old_rec = fh_->get_record(rid, context_);

            // 从索引中删除旧key
            for (size_t i = 0; i < tab_.indexes.size(); i++) {
                auto &index = tab_.indexes[i];
                auto ih = sm_manager_->ihs_
                              .at(sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols))
                              .get();
                char *old_key = new char[index.col_tot_len];
                int offset = 0;
                for (size_t j = 0; j < static_cast<size_t>(index.col_num); j++) {
                    memcpy(old_key + offset, old_rec->data + index.cols[j].offset, index.cols[j].len);
                    offset += index.cols[j].len;
                }
                ih->delete_entry(old_key, context_->txn_);
                delete[] old_key;
            }

            // 构建新记录：复制旧记录，然后应用SET子句
            auto new_rec = std::make_unique<RmRecord>(fh_->get_file_hdr().record_size);
            memcpy(new_rec->data, old_rec->data, fh_->get_file_hdr().record_size);
            for (auto &set_clause : set_clauses_) {
                auto &lhs_col = set_clause.lhs;
                auto &rhs_val = set_clause.rhs;
                // 查找列偏移
                for (auto &col : tab_.cols) {
                    if (col.name == lhs_col.col_name) {
                        if (col.not_null && rhs_val.is_null_) {
                            throw RMDBError("Column '" + col.name + "' cannot be NULL");
                        }
                        // INT->FLOAT implicit conversion
                        if (col.type == TYPE_FLOAT && rhs_val.type == TYPE_INT)
                            rhs_val.set_float((float)rhs_val.int_val);
                        rhs_val.init_raw(col.len);
                        memcpy(new_rec->data + col.offset, rhs_val.raw->data, col.len);
                        break;
                    }
                }
            }

            // 更新记录
            fh_->update_record(rid, new_rec->data, context_);

            // WAL: log the update (old + new values, chain prev_lsn for undo traversal)
            auto* update_log = new UpdateLogRecord(context_->txn_->get_transaction_id(), *old_rec, *new_rec, rid, tab_name_);
            update_log->prev_lsn_ = context_->txn_->get_prev_lsn();
            auto update_lsn = context_->log_mgr_->add_log_to_buffer(update_log);
            context_->txn_->set_prev_lsn(update_lsn);

            // Record for rollback (store old value for undo)
            auto* wr = new WriteRecord(WType::UPDATE_TUPLE, tab_name_, rid, *old_rec);
            context_->txn_->append_write_record(wr);

            // 在索引中插入新key
            for (size_t i = 0; i < tab_.indexes.size(); i++) {
                auto &index = tab_.indexes[i];
                auto ih = sm_manager_->ihs_
                              .at(sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols))
                              .get();
                char *new_key = new char[index.col_tot_len];
                int offset = 0;
                for (size_t j = 0; j < static_cast<size_t>(index.col_num); j++) {
                    memcpy(new_key + offset, new_rec->data + index.cols[j].offset, index.cols[j].len);
                    offset += index.cols[j].len;
                }
                ih->insert_entry(new_key, rid, context_->txn_);
                delete[] new_key;
            }
        }
        return nullptr;
    }

    Rid &rid() override { return _abstract_rid; }
};