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

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "transaction/transaction.h"
#include "transaction/concurrency/lock_manager.h"
#include "recovery/log_manager.h"

// class TransactionManager;

// used for data_send
static int const_offset = -1;

// ============================================================================
// SimpleArena — monotonic bump-pointer allocator.
// O(1) allocation, no internal locks, bulk reset per query.
// Eliminates per-tuple malloc/free contention under high concurrency.
//
// Safety guarantees:
//  - Oversized requests (> 32 KB) get a dedicated malloc block so they can
//    never exhaust the bump buffer for smaller hot-path allocations.
//  - When the bump buffer is full, additional requests fall back to malloc
//    and are tracked in an overflow list.  Reset() / destructor clean them up.
//  - No pointer can outlive the arena — all overflow blocks are freed together.
// ============================================================================
class SimpleArena {
   public:
    static constexpr size_t ARENA_SIZE = 131072;  // 128 KB per query

    SimpleArena() : buf_(new char[ARENA_SIZE]), offset_(0) {
        overflow_.reserve(4);  // typical query never overflows
    }
    ~SimpleArena() {
        delete[] buf_;
        for (char *p : overflow_) delete[] p;
    }
    SimpleArena(const SimpleArena &) = delete;
    SimpleArena &operator=(const SimpleArena &) = delete;

    // Returns a pointer to at least |size| bytes, or nullptr on OOM.
    // Never overruns — oversized or overflow requests go to the heap.
    char *Allocate(size_t size) {
        // Giant single request: don't exhaust the bump buffer for one object.
        // Route it directly to a tracked heap block.
        if (size > ARENA_SIZE / 4) {
            char *p = new (std::nothrow) char[size];
            if (p) overflow_.push_back(p);
            return p;
        }
        // Bump-allocate from the main buffer.
        if (offset_ + size <= ARENA_SIZE) {
            char *ptr = buf_ + offset_;
            offset_ += size;
            return ptr;
        }
        // Buffer exhausted — fall back to a tracked heap block.
        char *p = new (std::nothrow) char[size];
        if (p) overflow_.push_back(p);
        return p;
    }

    // Reset for the next query: rewind the bump pointer and free all overflow
    // blocks.  The main buffer is reused.
    void Reset() {
        offset_ = 0;
        for (char *p : overflow_) delete[] p;
        overflow_.clear();
    }

   private:
    char *buf_;
    size_t offset_;
    std::vector<char *> overflow_;  // heap blocks for oversized / overflow requests
};

class Context {
public:
    Context (LockManager *lock_mgr, LogManager *log_mgr,
            Transaction *txn, char *data_send = nullptr, int *offset = &const_offset)
        : lock_mgr_(lock_mgr), log_mgr_(log_mgr), txn_(txn),
          txn_mgr_(nullptr), data_send_(data_send), offset_(offset) {
            ellipsis_ = false;
            is_for_update_ = false;
          }

    LockManager *lock_mgr_;
    LogManager *log_mgr_;
    Transaction *txn_;
    class TransactionManager *txn_mgr_;
    char *data_send_;
    int *offset_;
    bool ellipsis_;
    bool is_for_update_;
    SimpleArena arena_;  // per-query arena for zero-lock tuple allocations
};