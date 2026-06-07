/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "lock_manager.h"

#include <chrono>
#include <functional>
#include <iomanip>
#include <sstream>
#include <thread>
#include <unordered_set>

LockManager::GroupLockMode LockManager::lock_mode_to_group_mode(LockMode mode) {
    switch (mode) {
        case LockMode::SHARED:              return GroupLockMode::S;
        case LockMode::EXLUCSIVE:           return GroupLockMode::X;
        case LockMode::INTENTION_SHARED:    return GroupLockMode::IS;
        case LockMode::INTENTION_EXCLUSIVE: return GroupLockMode::IX;
        case LockMode::S_IX:                return GroupLockMode::SIX;
        case LockMode::GAP:                 return GroupLockMode::GAP;
        case LockMode::NEXT_KEY:            return GroupLockMode::NEXT_KEY;
        case LockMode::INSERT_INTENTION:    return GroupLockMode::INSERT_INT;
        default:                            return GroupLockMode::NON_LOCK;
    }
}

bool LockManager::is_compatible(LockMode request_mode, GroupLockMode group_mode) {
    // Compatibility matrix: can request_mode be granted when group_mode is held?
    // LockMode order:       SHARED=0, EXCLUSIVE=1, IS=2, IX=3, S_IX=4, GAP=5, NEXT_KEY=6, INSERT_INT=7
    // GroupLockMode order:  NL=0, IS=1, IX=2, S=3, X=4, SIX=5, GAP=6, NEXT_KEY=7, INSERT_INT=8
    //
    // Rules:
    //  - GAP locks are compatible with each other (multiple txns can guard the same gap)
    //  - GAP conflicts only with INSERT_INTENTION and NEXT_KEY (and X of course)
    //  - INSERT_INTENTION is compatible with other INSERT_INTENTION (multiple waiters queue)
    //  - NEXT_KEY = X + GAP, conflicts like X on record + like GAP on gap
    //  - INSERT_INTENTION conflicts with GAP and NEXT_KEY (cannot insert into a guarded gap)
    static const bool compat[8][9] = {
        //         NL  IS  IX   S   X  SIX GAP NKY IIN
        /* S     */{ 1,  1,  0,  1,  0,  0,  0,  0,  0 },
        /* X     */{ 1,  0,  0,  0,  0,  0,  0,  0,  0 },
        /* IS    */{ 1,  1,  1,  1,  0,  1,  1,  0,  1 },
        /* IX    */{ 1,  1,  1,  0,  0,  0,  1,  0,  1 },
        /* S_IX  */{ 1,  1,  0,  0,  0,  0,  0,  0,  0 },
        /* GAP   */{ 1,  1,  1,  1,  0,  0,  1,  0,  0 },
        /* NEXT_KEY */{ 1, 0,  0,  0,  0,  0,  0,  0,  0 },
        /* INS_INT */{ 1, 1,  1,  1,  0,  0,  0,  0,  1 },
    };
    int row = static_cast<int>(request_mode);
    int col = static_cast<int>(group_mode);
    return compat[row][col];
}

void LockManager::update_group_lock_mode(LockRequestQueue& queue) {
    bool has_S = false, has_IX = false, has_IS = false, has_X = false, has_SIX = false;
    bool has_GAP = false, has_NEXT_KEY = false, has_INSERT_INT = false;
    for (auto& req : queue.request_queue_) {
        if (!req.granted_) continue;
        switch (req.lock_mode_) {
            case LockMode::SHARED:              has_S = true; break;
            case LockMode::EXLUCSIVE:           has_X = true; break;
            case LockMode::INTENTION_SHARED:    has_IS = true; break;
            case LockMode::INTENTION_EXCLUSIVE: has_IX = true; break;
            case LockMode::S_IX:                has_SIX = true; break;
            case LockMode::GAP:                 has_GAP = true; break;
            case LockMode::NEXT_KEY:            has_NEXT_KEY = true; break;
            case LockMode::INSERT_INTENTION:    has_INSERT_INT = true; break;
        }
    }
    // Priority: strongest lock wins (X > NEXT_KEY > SIX > S > GAP > IX > IS > INSERT_INT)
    if (has_X)               queue.group_lock_mode_ = GroupLockMode::X;
    else if (has_NEXT_KEY)   queue.group_lock_mode_ = GroupLockMode::NEXT_KEY;
    else if (has_SIX)        queue.group_lock_mode_ = GroupLockMode::SIX;
    else if (has_S && has_IX) queue.group_lock_mode_ = GroupLockMode::SIX;
    else if (has_S)          queue.group_lock_mode_ = GroupLockMode::S;
    else if (has_GAP)        queue.group_lock_mode_ = GroupLockMode::GAP;
    else if (has_IX)         queue.group_lock_mode_ = GroupLockMode::IX;
    else if (has_IS)         queue.group_lock_mode_ = GroupLockMode::IS;
    else if (has_INSERT_INT) queue.group_lock_mode_ = GroupLockMode::INSERT_INT;
    else                     queue.group_lock_mode_ = GroupLockMode::NON_LOCK;
}

void LockManager::add_wait_edges(txn_id_t waiter, const LockRequestQueue& queue) {
    std::scoped_lock wl(waits_for_latch_);
    for (auto& req : queue.request_queue_) {
        if (req.granted_ && req.txn_id_ != waiter) {
            waits_for_[waiter].insert(req.txn_id_);
        }
    }
}

void LockManager::remove_all_edges(txn_id_t txn) {
    std::scoped_lock wl(waits_for_latch_);
    waits_for_.erase(txn);
    // Also remove incoming edges pointing to this txn
    for (auto& [waiter, holders] : waits_for_) {
        holders.erase(txn);
    }
}

bool LockManager::lock_common(Transaction* txn, const LockDataId& lock_data_id, LockMode lock_mode) {
    auto& shard = shards_[shard_of(lock_data_id)];
    std::unique_lock<std::mutex> lock(shard.latch_);

    auto& queue = shard.table_[lock_data_id];
    txn_id_t my_id = txn->get_transaction_id();

    // Check if this transaction already holds a lock on this item
    for (auto& req : queue.request_queue_) {
        if (req.txn_id_ == my_id && req.granted_) {
            // Same lock type
            if (req.lock_mode_ == lock_mode) {
                return true;
            }
            // Lock upgrade S to X
            if (req.lock_mode_ == LockMode::SHARED && lock_mode == LockMode::EXLUCSIVE) {
                bool can_upgrade = true;
                for (auto& other : queue.request_queue_) {
                    if (other.txn_id_ != my_id && other.granted_) {
                        can_upgrade = false;
                        break;
                    }
                }
                if (can_upgrade) {
                    req.lock_mode_ = LockMode::EXLUCSIVE;
                    update_group_lock_mode(queue);
                    return true;
                }
                throw TransactionAbortException(my_id, AbortReason::UPGRADE_CONFLICT);
            }
            if (req.lock_mode_ == LockMode::EXLUCSIVE) return true;
            if (req.lock_mode_ == LockMode::S_IX) {
                if (lock_mode == LockMode::SHARED || lock_mode == LockMode::INTENTION_EXCLUSIVE
                    || lock_mode == LockMode::INTENTION_SHARED) return true;
            }
            if (req.lock_mode_ == LockMode::INTENTION_EXCLUSIVE
                && lock_mode == LockMode::INTENTION_SHARED) return true;
        }
    }

    // Wait until the lock request is compatible with the current group lock mode
    while (!is_compatible(lock_mode, queue.group_lock_mode_)) {
        // Check for deadlock (inline)
        txn_id_t victim = find_deadlock_victim(my_id, queue);
        if (victim != INVALID_TXN_ID) {
            remove_all_edges(my_id);
            throw TransactionAbortException(victim, AbortReason::DEADLOCK_PREVENTION);
        }

        // Add waits-for edges BEFORE blocking
        add_wait_edges(my_id, queue);

        // Add an ungranted request so deadlock detection can see us
        LockRequest wait_req(my_id, lock_mode);
        wait_req.granted_ = false;
        queue.request_queue_.push_back(wait_req);

        // Block until something changes in this lock queue
        queue.cv_.wait(lock);

        // Remove our ungranted request
        for (auto it = queue.request_queue_.begin(); it != queue.request_queue_.end(); ++it) {
            if (it->txn_id_ == my_id && !it->granted_) {
                queue.request_queue_.erase(it);
                break;
            }
        }

        // Remove waits-for edges after waking
        remove_all_edges(my_id);

        // Check if background detector marked us as victim
        {
            std::scoped_lock vlock(victims_mutex_);
            if (victims_.count(my_id)) {
                victims_.erase(my_id);
                throw TransactionAbortException(my_id, AbortReason::DEADLOCK_PREVENTION);
            }
        }
    }

    // Grant the lock
    LockRequest new_req(my_id, lock_mode);
    new_req.granted_ = true;
    queue.request_queue_.push_back(new_req);
    update_group_lock_mode(queue);
    txn->get_lock_set()->insert(lock_data_id);

    return true;
}

/**
 * @description: 申请行级共享锁
 */
bool LockManager::lock_shared_on_record(Transaction* txn, const Rid& rid, int tab_fd) {
    LockDataId lock_data_id(tab_fd, rid, LockDataType::RECORD);
    return lock_common(txn, lock_data_id, LockMode::SHARED);
}

/**
 * @description: 申请行级排他锁
 */
bool LockManager::lock_exclusive_on_record(Transaction* txn, const Rid& rid, int tab_fd) {
    txn->set_read_only(false);  // Mark as read-write txn for MVCC
    LockDataId lock_data_id(tab_fd, rid, LockDataType::RECORD);
    return lock_common(txn, lock_data_id, LockMode::EXLUCSIVE);
}

/**
 * @description: 申请表级读锁
 */
bool LockManager::lock_shared_on_table(Transaction* txn, int tab_fd) {
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    return lock_common(txn, lock_data_id, LockMode::SHARED);
}

/**
 * @description: 申请表级写锁
 */
bool LockManager::lock_exclusive_on_table(Transaction* txn, int tab_fd) {
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    return lock_common(txn, lock_data_id, LockMode::EXLUCSIVE);
}

/**
 * @description: 申请表级意向读锁
 */
bool LockManager::lock_IS_on_table(Transaction* txn, int tab_fd) {
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    return lock_common(txn, lock_data_id, LockMode::INTENTION_SHARED);
}

/**
 * @description: 申请表级意向写锁
 */
bool LockManager::lock_IX_on_table(Transaction* txn, int tab_fd) {
    txn->set_read_only(false);  // Mark as read-write txn for MVCC
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    return lock_common(txn, lock_data_id, LockMode::INTENTION_EXCLUSIVE);
}

/**
 * @description: 申请间隙锁（Gap Lock），锁住 rid 对应记录之前的间隙。
 *               多个事务可同时持有同一间隙锁（互相兼容），
 *               但与 INSERT_INTENTION 冲突，阻止并发插入。
 */
bool LockManager::lock_gap(Transaction* txn, const Rid& rid, int tab_fd) {
    txn->set_read_only(false);
    LockDataId lock_data_id(tab_fd, rid, LockDataType::GAP);
    return lock_common(txn, lock_data_id, LockMode::GAP);
}

/**
 * @description: 申请临键锁（Next-Key Lock），同时锁定记录（X锁）及其前驱间隙（GAP锁）。
 *               用于范围扫描的锁定读（SELECT ... FOR UPDATE），阻止幻读。
 *               同时获取 X lock + GAP lock 两个独立的锁。
 */
bool LockManager::lock_next_key(Transaction* txn, const Rid& rid, int tab_fd) {
    txn->set_read_only(false);
    // Acquire record X-lock first
    LockDataId rec_id(tab_fd, rid, LockDataType::RECORD);
    if (!lock_common(txn, rec_id, LockMode::EXLUCSIVE)) return false;
    // Then acquire gap lock on the gap before this record
    LockDataId gap_id(tab_fd, rid, LockDataType::GAP);
    return lock_common(txn, gap_id, LockMode::GAP);
}

/**
 * @description: 申请插入意向锁（Insert Intention Lock）。
 *               在 INSERT 执行前调用，检查目标位置是否被 GAP 锁覆盖。
 *               与 GAP / NEXT_KEY 冲突，多个插入意向锁互相兼容。
 */
bool LockManager::lock_insert_intention(Transaction* txn, const Rid& rid, int tab_fd) {
    txn->set_read_only(false);
    LockDataId lock_data_id(tab_fd, rid, LockDataType::GAP);
    return lock_common(txn, lock_data_id, LockMode::INSERT_INTENTION);
}

// ============================================================================
// Index-Key-based Gap Lock methods
// ============================================================================

/**
 * @description: 基于索引键的间隙锁（Gap Lock on Index Key）。
 *               锁住由 key_data 标识的索引键之前的逻辑间隙。
 *               多个事务可同时持有同一间隙锁（互相兼容），
 *               但与 INSERT_INTENTION 冲突。
 */
bool LockManager::lock_gap_on_key(Transaction* txn, int tab_fd, int index_id,
                                   const char* key_data, int key_len) {
    txn->set_read_only(false);
    LockDataId lock_data_id(tab_fd, index_id, key_data, key_len, LockDataType::GAP);
    return lock_common(txn, lock_data_id, LockMode::GAP);
}

/**
 * @description: 基于索引键的临键锁（Next-Key Lock on Index Key）。
 *               同时锁定记录（X锁，基于RID）及其前驱逻辑间隙（GAP锁，基于索引键）。
 *               用于范围扫描的锁定读（SELECT ... FOR UPDATE），阻止幻读。
 */
bool LockManager::lock_next_key_on_key(Transaction* txn, const Rid& rid, int tab_fd,
                                        int index_id, const char* key_data, int key_len) {
    txn->set_read_only(false);
    // Acquire record X-lock first (RID-based)
    LockDataId rec_id(tab_fd, rid, LockDataType::RECORD);
    if (!lock_common(txn, rec_id, LockMode::EXLUCSIVE)) return false;
    // Then acquire index-key-based GAP lock on the gap before this key
    LockDataId gap_id(tab_fd, index_id, key_data, key_len, LockDataType::GAP);
    return lock_common(txn, gap_id, LockMode::GAP);
}

/**
 * @description: 基于索引键的插入意向锁（Insert Intention Lock on Index Key）。
 *               在 INSERT 执行前调用，传入新记录在索引中的逻辑键值。
 *               与同一键值上的 GAP / NEXT_KEY 锁冲突，防止幻读插入。
 */
bool LockManager::lock_insert_intention_on_key(Transaction* txn, int tab_fd, int index_id,
                                                const char* key_data, int key_len) {
    txn->set_read_only(false);
    LockDataId lock_data_id(tab_fd, index_id, key_data, key_len, LockDataType::GAP);
    return lock_common(txn, lock_data_id, LockMode::INSERT_INTENTION);
}

/**
 * @description: 释放锁
 */
txn_id_t LockManager::find_deadlock_victim(txn_id_t requestor, const LockRequestQueue& queue) {
    // Collect all txn_ids that hold locks on this item (blocking the requestor)
    std::unordered_set<txn_id_t> blockers;
    for (auto& req : queue.request_queue_) {
        if (req.granted_ && req.txn_id_ != requestor) {
            blockers.insert(req.txn_id_);
        }
    }
    if (blockers.empty()) return INVALID_TXN_ID;

    // Build wait-for graph from all lock queues across all shards. Edge: waiter -> holder.
    // Best-effort snapshot — we only hold one shard latch; other shards may race.
    std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> wait_for;
    for (int si = 0; si < LOCK_TABLE_SHARDS; ++si) { auto& sh = shards_[si];
        for (auto& [id, q] : sh.table_) {
            std::unordered_set<txn_id_t> held_by;
            for (auto& req : q.request_queue_) {
                if (req.granted_) held_by.insert(req.txn_id_);
            }
            for (auto& req : q.request_queue_) {
                if (!req.granted_) {
                    for (auto holder : held_by) {
                        if (holder != req.txn_id_) {
                            wait_for[req.txn_id_].insert(holder);
                        }
                    }
                }
            }
        }
    }
    // Add temporary edges: if requestor were waiting, it would point to each blocker
    for (auto blocker : blockers) {
        wait_for[requestor].insert(blocker);
    }

    // DFS cycle detection. Collect all transactions in the cycle.
    std::unordered_set<txn_id_t> visited, rec_stack;
    std::vector<txn_id_t> cycle_members;

    std::function<bool(txn_id_t)> dfs = [&](txn_id_t u) -> bool {
        visited.insert(u);
        rec_stack.insert(u);
        for (auto v : wait_for[u]) {
            if (rec_stack.count(v)) {
                // Cycle found: v is already in the recursion stack.
                // Collect all transactions currently in the rec_stack (on the path from v to u).
                cycle_members.push_back(v);
                for (auto txn : rec_stack) {
                    if (txn != v) cycle_members.push_back(txn);
                }
                return true;
            }
            if (!visited.count(v) && dfs(v)) return true;
        }
        rec_stack.erase(u);
        return false;
    };

    bool has_cycle = dfs(requestor);
    // Remove temporary edges
    wait_for[requestor].clear();

    if (!has_cycle) return INVALID_TXN_ID;

    // Victim selection: choose the youngest transaction (highest txn_id) in the cycle.
    txn_id_t victim = INVALID_TXN_ID;
    for (auto txn : cycle_members) {
        if (txn > victim) victim = txn;
    }
    return victim;
}

bool LockManager::unlock(Transaction* txn, LockDataId lock_data_id) {
    auto& shard = shards_[shard_of(lock_data_id)];
    std::unique_lock<std::mutex> lock(shard.latch_);

    auto it = shard.table_.find(lock_data_id);
    if (it == shard.table_.end()) {
        return false;
    }

    auto& queue = it->second;
    auto& requests = queue.request_queue_;
    txn_id_t txn_id = txn->get_transaction_id();

    for (auto req_it = requests.begin(); req_it != requests.end(); ++req_it) {
        if (req_it->txn_id_ == txn_id && req_it->granted_) {
            requests.erase(req_it);
            update_group_lock_mode(queue);

            // Wake up waiters on this queue since group lock mode may have changed
            queue.cv_.notify_all();

            // Clean up empty queues
            if (requests.empty()) {
                shard.table_.erase(it);
            }

            return true;
        }
    }

    return false;
}

// ============================================================================
// Background deadlock detector
// ============================================================================

void LockManager::CheckDeadlock() {
    // Snapshot the waits-for graph under its latch
    std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> graph;
    {
        std::scoped_lock wl(waits_for_latch_);
        graph = waits_for_;
    }
    if (graph.empty()) return;

    // DFS with three-color marking: 0=unvisited, 1=in-stack, 2=done
    std::unordered_map<txn_id_t, int> color;
    std::unordered_map<txn_id_t, txn_id_t> parent;
    std::vector<txn_id_t> cycle;

    std::function<bool(txn_id_t)> dfs = [&](txn_id_t u) -> bool {
        color[u] = 1;  // in recursion stack
        for (auto v : graph[u]) {
            if (color[v] == 1) {
                // Back edge: cycle found. Walk parent chain to collect members.
                cycle.push_back(v);
                txn_id_t cur = u;
                while (cur != v) {
                    cycle.push_back(cur);
                    auto it = parent.find(cur);
                    if (it == parent.end() || it->second == cur) break;
                    cur = it->second;
                }
                return true;
            }
            if (color[v] == 0) {
                parent[v] = u;
                if (dfs(v)) return true;
            }
        }
        color[u] = 2;  // done
        return false;
    };

    bool has_cycle = false;
    for (auto& [u, _] : graph) {
        if (color[u] == 0) {
            if (dfs(u)) { has_cycle = true; break; }
        }
    }

    if (!has_cycle) return;

    // Victim: youngest (highest txn_id) in the cycle
    txn_id_t victim = INVALID_TXN_ID;
    for (auto id : cycle) {
        if (id > victim) victim = id;
    }
    if (victim == INVALID_TXN_ID) return;
    {
        std::scoped_lock vlock(victims_mutex_);
        victims_.insert(victim);
    }

    // Find the queue where the victim has an ungranted request and wake it.
    // Iterate all shards; briefly lock each to safely inspect its table.
    for (int si = 0; si < LOCK_TABLE_SHARDS; ++si) { auto& sh = shards_[si];
        std::scoped_lock slock(sh.latch_);
        for (auto& [id, queue] : sh.table_) {
            for (auto& req : queue.request_queue_) {
                if (req.txn_id_ == victim && !req.granted_) {
                    queue.cv_.notify_all();
                    goto done_wake;
                }
            }
        }
    }
    done_wake:;

    // Clean up the victim's waits-for edges
    remove_all_edges(victim);
}

void LockManager::detector_loop() {
    while (!stop_detector_.load()) {
        {
            std::unique_lock<std::mutex> lock(detector_mutex_);
            detector_cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return stop_detector_.load();
            });
        }
        if (stop_detector_.load()) break;
        CheckDeadlock();
    }
}

void LockManager::start_deadlock_detector() {
    if (detector_thread_.joinable()) return;
    stop_detector_ = false;
    detector_thread_ = std::thread(&LockManager::detector_loop, this);
}

void LockManager::stop_deadlock_detector() {
    stop_detector_ = true;
    detector_cv_.notify_all();
    if (detector_thread_.joinable()) {
        detector_thread_.join();
    }
}

std::string LockManager::DumpLockGraph() {
    std::stringstream ss;
    ss << "+------------------+------------------+------------------+------------------+\n";
    ss << "| txn_id          | lock_data        | mode             | granted         |\n";
    ss << "+------------------+------------------+------------------+------------------+\n";

    bool has_rows = false;
    for (int i = 0; i < LOCK_TABLE_SHARDS; i++) {
        std::lock_guard<std::mutex> guard(shards_[i].latch_);
        for (auto &kv : shards_[i].table_) {
            const LockDataId &id = kv.first;
            const LockRequestQueue &queue = kv.second;
            for (const auto &req : queue.request_queue_) {
                has_rows = true;
                ss << "| " << std::setw(16) << req.txn_id_
                   << " | " << std::setw(16) << id.Get()
                   << " | " << std::setw(16) << (req.granted_ ? "GRANTED" : "WAITING")
                   << " | " << std::setw(16) << (req.granted_ ? "yes" : "no")
                   << " |\n";
            }
        }
    }

    if (has_rows) {
        ss << "+------------------+------------------+------------------+------------------+\n";
    } else {
        ss << "| (no locks held)                                                  |\n";
        ss << "+------------------+------------------+------------------+------------------+\n";
    }

    // Add wait-for graph summary
    {
        std::lock_guard<std::mutex> guard(waits_for_latch_);
        if (!waits_for_.empty()) {
            ss << "\nWait-for edges:\n";
            ss << "+------------------+------------------+\n";
            ss << "| waiter           | holders          |\n";
            ss << "+------------------+------------------+\n";
            for (auto &kv : waits_for_) {
                for (auto holder : kv.second) {
                    ss << "| " << std::setw(16) << kv.first
                       << " | " << std::setw(16) << holder << " |\n";
                }
            }
            ss << "+------------------+------------------+\n";
        }
    }

    // Deadlock victims
    {
        std::lock_guard<std::mutex> guard(victims_mutex_);
        if (!victims_.empty()) {
            ss << "Deadlock victims: ";
            for (auto v : victims_) ss << v << " ";
            ss << "\n";
        }
    }

    return ss.str();
}
