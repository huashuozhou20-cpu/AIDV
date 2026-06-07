/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "log_recovery.h"

#include <fstream>
#include <unordered_set>

#include "record/rm_file_handle.h"

void RecoveryManager::read_log_file() {
    log_size_ = disk_manager_->get_file_size(LOG_FILE_NAME);
    if (log_size_ <= 0) {
        log_data_ = nullptr;
        return;
    }
    log_data_ = new char[log_size_];
    disk_manager_->read_log(log_data_, log_size_, 0);
}

const char* RecoveryManager::get_record_by_lsn(lsn_t lsn) const {
    auto it = lsn_to_offset_.find(lsn);
    if (it == lsn_to_offset_.end()) {
        return nullptr;
    }
    return log_data_ + it->second;
}

void RecoveryManager::undo_txn(txn_id_t txn_id, lsn_t start_lsn) {
    lsn_t lsn = start_lsn;
    while (lsn != INVALID_LSN) {
        const char* rec = get_record_by_lsn(lsn);
        if (rec == nullptr) break;

        LogType log_type = *reinterpret_cast<const LogType*>(rec + OFFSET_LOG_TYPE);
        if (log_type == LogType::begin) break;

        if (log_type == LogType::INSERT) {
            InsertLogRecord log_rec;
            log_rec.deserialize(rec);
            auto it = sm_manager_->fhs_.find(std::string(log_rec.table_name_, log_rec.table_name_size_));
            if (it != sm_manager_->fhs_.end()) {
                auto* fh = it->second.get();
                if (log_rec.rid_.page_no < fh->get_file_hdr().num_pages) {
                    // Idempotent undo: verify the record at this slot actually
                    // belongs to the transaction being undone. If the slot was
                    // reused by a committed transaction (T2), skip it.
                    auto rec_at_slot = fh->get_record_snapshot(log_rec.rid_);
                    if (rec_at_slot) {
                        txn_id_t slot_txn;
                        memcpy(&slot_txn, rec_at_slot->data + trx_id_offset(rec_at_slot->size),
                               sizeof(txn_id_t));
                        // Only delete if the record's txn_id matches the undone txn
                        if (slot_txn == txn_id) {
                            fh->delete_record(log_rec.rid_, nullptr);
                        }
                    } else {
                        fh->delete_record(log_rec.rid_, nullptr);
                    }
                }
            }
        } else if (log_type == LogType::DELETE) {
            DeleteLogRecord log_rec;
            log_rec.deserialize(rec);
            auto it = sm_manager_->fhs_.find(std::string(log_rec.table_name_, log_rec.table_name_size_));
            if (it != sm_manager_->fhs_.end()) {
                auto* fh = it->second.get();
                while (fh->get_file_hdr().num_pages <= log_rec.rid_.page_no) {
                    RmPageHandle ph = fh->create_new_page_handle();
                    buffer_pool_manager_->unpin_page(ph.page->get_page_id(), true);
                }
                // Only re-insert if the slot is empty (idempotent undo)
                auto existing = fh->get_record_snapshot(log_rec.rid_);
                if (!existing) {
                    fh->insert_record(log_rec.rid_, log_rec.delete_value_.data);
                }
            }
        } else if (log_type == LogType::UPDATE) {
            UpdateLogRecord log_rec;
            log_rec.deserialize(rec);
            auto it = sm_manager_->fhs_.find(std::string(log_rec.table_name_, log_rec.table_name_size_));
            if (it != sm_manager_->fhs_.end()) {
                auto* fh = it->second.get();
                if (log_rec.rid_.page_no < fh->get_file_hdr().num_pages) {
                    // Idempotent undo: only restore old value if current record
                    // belongs to the transaction being undone
                    auto rec_at_slot = fh->get_record_snapshot(log_rec.rid_);
                    if (rec_at_slot) {
                        txn_id_t slot_txn;
                        memcpy(&slot_txn, rec_at_slot->data + trx_id_offset(rec_at_slot->size),
                               sizeof(txn_id_t));
                        if (slot_txn == txn_id) {
                            fh->update_record(log_rec.rid_, log_rec.old_value_.data, nullptr);
                        }
                    } else {
                        fh->update_record(log_rec.rid_, log_rec.old_value_.data, nullptr);
                    }
                }
            }
        }

        lsn = *reinterpret_cast<const lsn_t*>(rec + OFFSET_PREV_LSN);
    }
}

/**
 * @description: analyze阶段，需要获得脏页表（DPT）和未完成的事务列表（ATT）
 */
void RecoveryManager::analyze() {
    read_log_file();
    if (log_data_ == nullptr) return;

    att_.clear();
    txn_last_lsn_.clear();
    lsn_to_offset_.clear();
    dpt_.clear();
    max_lsn_ = INVALID_LSN;

    int offset = 0;
    while (offset < log_size_) {
        const char* rec = log_data_ + offset;
        LogType log_type = *reinterpret_cast<const LogType*>(rec + OFFSET_LOG_TYPE);
        lsn_t lsn = *reinterpret_cast<const lsn_t*>(rec + OFFSET_LSN);
        if (lsn > max_lsn_) max_lsn_ = lsn;
        uint32_t log_tot_len = *reinterpret_cast<const uint32_t*>(rec + OFFSET_LOG_TOT_LEN);
        txn_id_t log_tid = *reinterpret_cast<const txn_id_t*>(rec + OFFSET_LOG_TID);

        lsn_to_offset_[lsn] = offset;
        txn_last_lsn_[log_tid] = lsn;

        switch (log_type) {
            case LogType::begin:
                att_.insert(log_tid);
                break;
            case LogType::commit:
            case LogType::ABORT:
                att_.erase(log_tid);
                break;
            case LogType::INSERT:
            case LogType::DELETE:
            case LogType::UPDATE: {
                // Extract page_no from the record's rid
                int record_size = *reinterpret_cast<const int*>(rec + OFFSET_LOG_DATA);
                int rid_offset = OFFSET_LOG_DATA + sizeof(int) + record_size;
                if (log_type == LogType::UPDATE) {
                    // UPDATE has two records: old_value and new_value
                    int new_record_size = *reinterpret_cast<const int*>(rec + rid_offset);
                    rid_offset += sizeof(int) + new_record_size;
                }
                const Rid* rid = reinterpret_cast<const Rid*>(rec + rid_offset);
                dpt_[rid->page_no].push_back(lsn);
                break;
            }
        }

        offset += log_tot_len;
    }
}

/**
 * @description: 重做所有未落盘的操作 — with page-LSN idempotence barrier.
 *               If a page already has LSN >= log record LSN, the change was
 *               already applied (by a previous recovery attempt that crashed
 *               mid-redo).  Skip it to avoid double-apply corruption.
 */
void RecoveryManager::redo() {
    if (log_data_ == nullptr) return;

    int offset = 0;
    while (offset < log_size_) {
        const char* rec = log_data_ + offset;
        LogType log_type = *reinterpret_cast<const LogType*>(rec + OFFSET_LOG_TYPE);
        lsn_t lsn = *reinterpret_cast<const lsn_t*>(rec + OFFSET_LSN);
        uint32_t log_tot_len = *reinterpret_cast<const uint32_t*>(rec + OFFSET_LOG_TOT_LEN);

        if (log_type == LogType::INSERT) {
            InsertLogRecord log_rec;
            log_rec.deserialize(rec);
            std::string table_name(log_rec.table_name_, log_rec.table_name_size_);
            auto it = sm_manager_->fhs_.find(table_name);
            if (it != sm_manager_->fhs_.end()) {
                auto* fh = it->second.get();
                // Ensure the page exists on disk before we try to read it
                int target_pages = log_rec.rid_.page_no + 1;
                if (fh->get_file_hdr().num_pages < target_pages) {
                    fh->set_num_pages(target_pages);
                }
                disk_manager_->ensure_pages(fh->GetFd(), target_pages);
                // Page-LSN idempotence barrier: skip if already applied
                PageId pid{fh->GetFd(), log_rec.rid_.page_no};
                Page* page = buffer_pool_manager_->fetch_page(pid);
                if (page->get_page_lsn() >= lsn) {
                    buffer_pool_manager_->unpin_page(pid, false);
                    offset += log_tot_len;
                    continue;
                }
                buffer_pool_manager_->unpin_page(pid, false);
                fh->insert_record(log_rec.rid_, log_rec.insert_value_.data);
                // Stamp the page with this log record's LSN
                page = buffer_pool_manager_->fetch_page(pid);
                page->set_page_lsn(lsn);
                buffer_pool_manager_->unpin_page(pid, true);
            }
        } else if (log_type == LogType::DELETE) {
            DeleteLogRecord log_rec;
            log_rec.deserialize(rec);
            std::string table_name(log_rec.table_name_, log_rec.table_name_size_);
            auto it = sm_manager_->fhs_.find(table_name);
            if (it != sm_manager_->fhs_.end()) {
                auto* fh = it->second.get();
                int target_pages = log_rec.rid_.page_no + 1;
                if (fh->get_file_hdr().num_pages < target_pages) {
                    fh->set_num_pages(target_pages);
                }
                disk_manager_->ensure_pages(fh->GetFd(), target_pages);
                PageId pid{fh->GetFd(), log_rec.rid_.page_no};
                Page* page = buffer_pool_manager_->fetch_page(pid);
                if (page->get_page_lsn() >= lsn) {
                    buffer_pool_manager_->unpin_page(pid, false);
                    offset += log_tot_len;
                    continue;
                }
                buffer_pool_manager_->unpin_page(pid, false);
                // The slot may already be empty (idempotent redo: the
                // INSERT before this DELETE may have been skipped because
                // the page LSN already covered it).
                if (fh->is_record(log_rec.rid_)) {
                    fh->delete_record(log_rec.rid_, nullptr);
                }
                page = buffer_pool_manager_->fetch_page(pid);
                page->set_page_lsn(lsn);
                buffer_pool_manager_->unpin_page(pid, true);
            }
        } else if (log_type == LogType::UPDATE) {
            UpdateLogRecord log_rec;
            log_rec.deserialize(rec);
            std::string table_name(log_rec.table_name_, log_rec.table_name_size_);
            auto it = sm_manager_->fhs_.find(table_name);
            if (it != sm_manager_->fhs_.end()) {
                auto* fh = it->second.get();
                int target_pages = log_rec.rid_.page_no + 1;
                if (fh->get_file_hdr().num_pages < target_pages) {
                    fh->set_num_pages(target_pages);
                }
                disk_manager_->ensure_pages(fh->GetFd(), target_pages);
                PageId pid{fh->GetFd(), log_rec.rid_.page_no};
                Page* page = buffer_pool_manager_->fetch_page(pid);
                if (page->get_page_lsn() >= lsn) {
                    buffer_pool_manager_->unpin_page(pid, false);
                    offset += log_tot_len;
                    continue;
                }
                buffer_pool_manager_->unpin_page(pid, false);
                // Idempotent redo: only update if the record exists (the
                // INSERT may have been skipped because the page LSN was
                // already up-to-date from a previous recovery pass).
                if (fh->is_record(log_rec.rid_)) {
                    fh->update_record(log_rec.rid_, log_rec.new_value_.data, nullptr);
                }
                page = buffer_pool_manager_->fetch_page(pid);
                page->set_page_lsn(lsn);
                buffer_pool_manager_->unpin_page(pid, true);
            }
        }

        offset += log_tot_len;
    }

    // Persist file headers + dirty pages to disk so a nested crash
    // during undo can recover from the redo-completed state.
    for (auto& entry : sm_manager_->fhs_) {
        auto* fh = entry.second.get();
        fh->flush_file_hdr();  // write in-memory num_pages to disk
        buffer_pool_manager_->flush_all_pages(fh->GetFd());
    }
}

/**
 * @description: 回滚未完成的事务
 */
void RecoveryManager::undo() {
    if (log_data_ == nullptr) return;

    // Undo each active transaction (process in any order for now)
    for (txn_id_t txn_id : att_) {
        lsn_t last_lsn = txn_last_lsn_[txn_id];
        undo_txn(txn_id, last_lsn);
    }

    att_.clear();
    txn_last_lsn_.clear();
    lsn_to_offset_.clear();
    dpt_.clear();

    if (log_data_ != nullptr) {
        delete[] log_data_;
        log_data_ = nullptr;
    }
    log_size_ = 0;

    // NOTE: We intentionally do NOT truncate the WAL here.  If a nested
    // crash occurs during recovery, the next recovery pass needs the
    // original log records.  The log_manager appends new records after the
    // last valid LSN, so old records don't interfere.
}

/**
 * @description: Rebuild all B+tree indexes from the recovered record files.
 *               Because index operations are not WAL-logged, the indexes are
 *               stale after ARIES recovery.  We drop every index, recreate
 *               it, and re-insert every record from the recovered data file.
 */
void RecoveryManager::rebuild_indexes() {
    auto* ix_mgr = sm_manager_->get_ix_manager();
    auto& db = sm_manager_->db_;

    // ---- Atomic sentinel: if a previous rebuild was interrupted by a crash,
    //      the marker file will still exist and we know we must re-rebuild. ----
    std::string marker_path = sm_manager_->get_db_path() + "/.index_rebuild_in_progress";

    for (auto& tab_entry : db.tables()) {
        auto& tab = tab_entry.second;
        std::string tab_name = tab.name;

        // Skip tables that have no open record file handle
        auto fh_it = sm_manager_->fhs_.find(tab_name);
        if (fh_it == sm_manager_->fhs_.end()) continue;
        auto* fh = fh_it->second.get();

        // ---- Phase 1: drop and recreate every index on this table ---------
        for (auto& index : tab.indexes) {
            auto ix_name = ix_mgr->get_index_name(tab_name, index.cols);

            // Close the stale index handle
            auto ih_it = sm_manager_->ihs_.find(ix_name);
            if (ih_it != sm_manager_->ihs_.end()) {
                ix_mgr->close_index(ih_it->second.get());
                sm_manager_->ihs_.erase(ih_it);
            }

            // Destroy the stale index file on disk
            if (ix_mgr->exists(tab_name, index.cols)) {
                ix_mgr->destroy_index(tab_name, index.cols);
            }

            // Create a brand-new empty index
            ix_mgr->create_index(tab_name, index.cols);

            // Open the fresh index and store the handle
            auto ih = ix_mgr->open_index(tab_name, index.cols);
            sm_manager_->ihs_.emplace(ix_name, std::move(ih));

            // Write sentinel BEFORE scanning — if we crash during the scan
            // the file remains, and the next recovery will redo the rebuild.
            {
                std::ofstream ofs(marker_path, std::ios::trunc);
                ofs << tab_name << "\n" << ix_name << "\n";
            }
        }

        // ---- Phase 2: scan the recovered record file and re-insert -------
        RmScan scan(fh);
        int col_tot_len = 0;
        for (auto& col : tab.cols) col_tot_len += col.len;

        int count = 0;
        while (!scan.is_end()) {
            auto rec = fh->get_record(scan.rid(), nullptr);
            if (!rec) { scan.next(); continue; }

            // For each index, extract the key and insert
            for (auto& index : tab.indexes) {
                auto ix_name = ix_mgr->get_index_name(tab_name, index.cols);
                auto ih_it = sm_manager_->ihs_.find(ix_name);
                if (ih_it == sm_manager_->ihs_.end()) continue;
                auto* ih = ih_it->second.get();

                char* key = new char[index.col_tot_len];
                int offset = 0;
                for (auto& col : index.cols) {
                    memcpy(key + offset, rec->data + col.offset, col.len);
                    offset += col.len;
                }
                ih->insert_entry(key, scan.rid(), nullptr);
                delete[] key;
            }

            count++;
            scan.next();
        }
    }

    // Rebuild completed successfully — clear the sentinel.
    std::remove(marker_path.c_str());
}
