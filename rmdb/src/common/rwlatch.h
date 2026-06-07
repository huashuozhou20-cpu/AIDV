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
#include <immintrin.h>

// ============================================================================
// ReaderWriterSpinLatch — pure user-space read/write spinlock with version
// sequence for optimistic lock-free reads.
//
// State word layout (32 bits):
//   bit 31          — write-lock flag  (1 = writer active)
//   bits 30..0      — reader count     (0 = no readers)
//
// Sequence counter (64 bits, cache-line aligned):
//   Incremented on every WLock + WUnlock pair.  An odd value means a writer
//   is active.  An even value means the page is quiescent.  Optimistic readers
//   sample the sequence before and after reading page data; if the sequence
//   changed the data may be inconsistent and the read must be retried.
// ============================================================================
class ReaderWriterSpinLatch {
    static constexpr uint32_t WRITE_BIT  = 0x80000000u;
    static constexpr uint32_t READ_MASK  = 0x7FFFFFFFu;

    mutable std::atomic<uint32_t> state_{0};

    // Placed on a separate cache line to avoid false-sharing with state_
    // and the page data that surrounds this object.
    alignas(64) mutable std::atomic<uint64_t> sequence_{0};

    static inline void cpu_relax() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield");
#else
        // portable back-off
#endif
    }

   public:
    // ---- Shared (read) lock -------------------------------------------------

    void lock_shared() const {
        while (true) {
            uint32_t cur = state_.load(std::memory_order_relaxed);
            if (!(cur & WRITE_BIT)) {
                if (state_.compare_exchange_weak(cur, cur + 1,
                        std::memory_order_acquire, std::memory_order_relaxed)) {
                    return;
                }
            } else {
                cpu_relax();
            }
        }
    }

    void unlock_shared() const {
        state_.fetch_sub(1, std::memory_order_release);
    }

    // ---- Exclusive (write) lock ---------------------------------------------

    void lock() {
        while (true) {
            uint32_t cur = state_.load(std::memory_order_relaxed);
            if (cur == 0) {
                if (state_.compare_exchange_weak(cur, WRITE_BIT,
                        std::memory_order_acquire, std::memory_order_relaxed)) {
                    // Bump sequence to odd — signals "write in progress"
                    sequence_.fetch_add(1, std::memory_order_release);
                    return;
                }
            } else {
                cpu_relax();
            }
        }
    }

    void unlock() {
        // Bump sequence to even — signals "write complete, data is fresh"
        sequence_.fetch_add(1, std::memory_order_release);
        state_.store(0, std::memory_order_release);
    }

    // ---- Optimistic read support --------------------------------------------

    // Return the current version sequence.  An odd value means a writer is
    // active; an even value means the page is quiescent and can be read
    // optimistically (without acquiring rlock).
    //
    // Relaxed is safe here because the leaf rlock() in find_leaf_page provides
    // the necessary acquire barrier before data is read.  The lock-free path
    // uses GetSequence as a heuristic to detect concurrent splits — a false
    // negative (missed split) only causes a wasted descent that is caught by
    // the leaf rlock, and a false positive only causes a cheap restart.
    uint64_t GetSequence() const {
        return sequence_.load(std::memory_order_relaxed);
    }
};
