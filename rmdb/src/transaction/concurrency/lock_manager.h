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

#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "transaction/transaction.h"

static const std::string GroupLockModeStr[10] = {"NON_LOCK", "IS", "IX", "S", "X", "SIX", "GAP", "NEXT_KEY", "INSERT_INT"};

class LockManager {
    /* 加锁类型，包括共享锁、排他锁、意向共享锁、意向排他锁、SIX（意向排他锁+共享锁）、间隙锁、临键锁、插入意向锁 */
    enum class LockMode { SHARED, EXLUCSIVE, INTENTION_SHARED, INTENTION_EXCLUSIVE, S_IX,
                          GAP, NEXT_KEY, INSERT_INTENTION };

    /* 用于标识加锁队列中排他性最强的锁类型 */
    enum class GroupLockMode { NON_LOCK, IS, IX, S, X, SIX, GAP, NEXT_KEY, INSERT_INT };

    /* 事务的加锁申请 */
    class LockRequest {
    public:
        LockRequest(txn_id_t txn_id, LockMode lock_mode)
            : txn_id_(txn_id), lock_mode_(lock_mode), granted_(false) {}

        txn_id_t txn_id_;   // 申请加锁的事务ID
        LockMode lock_mode_;    // 事务申请加锁的类型
        bool granted_;          // 该事务是否已经被赋予锁
    };

    /* 数据项上的加锁队列 */
    class LockRequestQueue {
    public:
        std::list<LockRequest> request_queue_;  // 加锁队列
        std::condition_variable cv_;            // 条件变量，用于唤醒正在等待加锁的申请，在no-wait策略下无需使用
        GroupLockMode group_lock_mode_ = GroupLockMode::NON_LOCK;   // 加锁队列的锁模式
    };

public:
    LockManager() {
        shards_ = std::make_unique<LockTableShard[]>(LOCK_TABLE_SHARDS);
        start_deadlock_detector();
    }

    ~LockManager() { stop_deadlock_detector(); }

    bool lock_shared_on_record(Transaction* txn, const Rid& rid, int tab_fd);

    bool lock_exclusive_on_record(Transaction* txn, const Rid& rid, int tab_fd);

    bool lock_shared_on_table(Transaction* txn, int tab_fd);

    bool lock_exclusive_on_table(Transaction* txn, int tab_fd);

    bool lock_IS_on_table(Transaction* txn, int tab_fd);

    bool lock_IX_on_table(Transaction* txn, int tab_fd);

    // Gap lock / Next-Key lock / Insert Intention (phantom prevention)
    // Index-key-based variants: identify gaps by logical key, not physical RID
    bool lock_gap_on_key(Transaction* txn, int tab_fd, int index_id,
                         const char* key_data, int key_len);
    bool lock_next_key_on_key(Transaction* txn, const Rid& rid, int tab_fd,
                              int index_id, const char* key_data, int key_len);
    bool lock_insert_intention_on_key(Transaction* txn, int tab_fd, int index_id,
                                      const char* key_data, int key_len);

    // Legacy RID-based variants (for backwards compat, delegate to index-key if needed)
    bool lock_gap(Transaction* txn, const Rid& rid, int tab_fd);
    bool lock_next_key(Transaction* txn, const Rid& rid, int tab_fd);
    bool lock_insert_intention(Transaction* txn, const Rid& rid, int tab_fd);

    bool unlock(Transaction* txn, LockDataId lock_data_id);

    // Dump current lock graph state for SHOW LOCK GRAPH command.
    // Returns a formatted ASCII table of all lock queues.
    std::string DumpLockGraph();

    // Background deadlock detector — start/stop lifecycle
    void start_deadlock_detector();
    void stop_deadlock_detector();
    void CheckDeadlock();

private:
    // Sharded lock table: 131071 prime buckets, each with an independent mutex.
    // A LockDataId maps to exactly one shard via hash % LOCK_TABLE_SHARDS.
    // Only two threads contending for the same LockDataId serialise; all other
    // lock/unlock operations proceed in parallel across different shards.
    // Uses unique_ptr<T[]> (one-time startup allocation, no hot-path overhead)
    // because std::array<T,N> fails on std::mutex's protected default ctor in
    // some GCC/libstdc++ configurations.
    static constexpr int LOCK_TABLE_SHARDS = 131071;

    struct LockTableShard {
        std::mutex latch_;
        std::unordered_map<LockDataId, LockRequestQueue> table_;
    };
    std::unique_ptr<LockTableShard[]> shards_;

    size_t shard_of(const LockDataId& id) const {
        return std::hash<int64_t>{}(id.Get()) % LOCK_TABLE_SHARDS;
    }

    std::mutex victims_mutex_;   // protects victims_ set

    bool lock_common(Transaction* txn, const LockDataId& lock_data_id, LockMode lock_mode);
    void update_group_lock_mode(LockRequestQueue& queue);
    bool is_compatible(LockMode request_mode, GroupLockMode group_mode);
    GroupLockMode lock_mode_to_group_mode(LockMode mode);

    // Inline deadlock detection: find the best victim in the wait-for graph cycle.
    txn_id_t find_deadlock_victim(txn_id_t requestor, const LockRequestQueue& queue);

    // --- Background deadlock detector ---
    // Persistent waits-for graph: waiter → set of holders blocking it
    std::mutex waits_for_latch_;
    std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> waits_for_;

    // Transactions marked for abort by the background detector
    std::unordered_set<txn_id_t> victims_;

    // Background detector thread
    std::thread detector_thread_;
    std::atomic<bool> stop_detector_{false};
    std::condition_variable detector_cv_;
    std::mutex detector_mutex_;

    // Edge maintenance helpers
    void add_wait_edges(txn_id_t waiter, const LockRequestQueue& queue);
    void remove_all_edges(txn_id_t txn);

    void detector_loop();
};
