/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "transaction/watermark.h"


auto Watermark::AddTxn(timestamp_t read_ts) -> void {
    std::scoped_lock lock(mtx_);
    auto it = current_reads_.find(read_ts);
    if (it == current_reads_.end()) {
        bool was_empty = current_reads_.empty();
        current_reads_[read_ts] = 1;
        // If the map was empty, the watermark may be stale (left at an old
        // commit_ts_ by the last RemoveTxn). Always advance to the new
        // read_ts, which is now the sole active timestamp.
        // Otherwise, lower the watermark only if this is the new minimum.
        if (was_empty || read_ts < watermark_) {
            watermark_ = read_ts;
        }
    } else {
        // Multiple transactions can share the same read_ts (rare but possible
        // when next_timestamp_ wraps or in synthetic test scenarios).
        it->second++;
    }
}

auto Watermark::RemoveTxn(timestamp_t read_ts) -> void {
    std::scoped_lock lock(mtx_);
    auto it = current_reads_.find(read_ts);
    if (it == current_reads_.end()) {
        // Not found — can happen if the transaction was never added
        // or was already removed. Harmless no-op.
        return;
    }
    it->second--;
    if (it->second > 0) {
        // Other transactions still hold this read_ts. Watermark unchanged.
        return;
    }
    // Last transaction with this read_ts has finished.
    current_reads_.erase(it);
    if (current_reads_.empty()) {
        // No active transactions remain — advance watermark to the latest
        // known commit timestamp. This is the GC safety signal: all txns
        // with commit_ts below this watermark are invisible to every
        // possible reader.
        watermark_ = commit_ts_;
    } else {
        // Advance watermark to the next-smallest active read_ts.
        // std::map is ordered, so begin()->first is the minimum key.
        watermark_ = current_reads_.begin()->first;
    }
}

auto Watermark::UpdateCommitTs(timestamp_t commit_ts) -> void {
    std::scoped_lock lock(mtx_);
    // Enforce monotonicity: concurrent commits may call UpdateCommitTs
    // out of order (a later commit with higher ts may reach this method
    // before an earlier commit). We must never allow commit_ts_ to regress,
    // or the watermark would decrease and GC would prematurely purge
    // still-visible undo logs.
    if (commit_ts > commit_ts_) {
        commit_ts_ = commit_ts;
    }
    // If no active readers exist, advance the watermark to the latest
    // commit timestamp so GC can clean up.
    if (current_reads_.empty() && commit_ts_ > watermark_) {
        watermark_ = commit_ts_;
    }
}

auto Watermark::GetWatermark() const -> timestamp_t {
    std::scoped_lock lock(mtx_);
    return watermark_;
}