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

#include <map>
#include <mutex>
#include <unordered_map>

#include "transaction/transaction.h"


/**
 * @brief Tracks all active transaction read timestamps to compute
 *        the system watermark — the minimum read_ts among all in-flight
 *        transactions. The watermark is the safety threshold for GC:
 *        any committed/aborted txn with commit_ts below the watermark
 *        is no longer visible to any active reader and can be purged.
 *
 * Thread safety: all public methods are protected by mtx_.
 */
class Watermark {
public:
  explicit Watermark(timestamp_t commit_ts) : commit_ts_(commit_ts), watermark_(commit_ts) {}

  /** Register a new active transaction with the given read timestamp. */
  void AddTxn(timestamp_t read_ts);

  /** Unregister a transaction that is committing or aborting.
   *  When the last active txn is removed, the watermark advances to
   *  the latest known commit timestamp. */
  void RemoveTxn(timestamp_t read_ts);

  /** Update the latest commit timestamp (used to advance watermark
   *  when no active readers remain). Caller must ensure monotonicity
   *  — this method ignores non-monotonic updates. */
  void UpdateCommitTs(timestamp_t commit_ts);

  /** Return the current watermark: the minimum read_ts of all active
   *  transactions, or the latest commit_ts if none are active. */
  timestamp_t GetWatermark() const;

private:
  mutable std::mutex mtx_;           // protects all member access
  timestamp_t commit_ts_;            // latest commit timestamp (monotonic)
  timestamp_t watermark_;            // current system watermark

  /** Map from read_ts → count of active transactions holding that timestamp.
   *  Uses std::map (ordered) so begin()->first is the minimum. */
  std::map<timestamp_t, int> current_reads_;
};