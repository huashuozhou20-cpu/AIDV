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

#include <vector>
#include <optional>

#include "transaction/transaction.h"
#include "transaction/transaction_manager.h"
#include "common/common.h"

// Check if a write-write conflict exists: another transaction committed a change
// to this tuple after the current transaction's read timestamp.
inline auto IsWriteWriteConflict(timestamp_t tuple_ts, Transaction *txn) -> bool {
    // If the tuple was committed after our read timestamp and it's not our own write,
    // we have a write-write conflict.
    timestamp_t read_ts = txn->get_read_ts();
    return tuple_ts != INVALID_TS && read_ts != INVALID_TS && tuple_ts > read_ts;
}

// Reconstruct a tuple by applying undo logs (in reverse order) to the base tuple.
// Returns std::nullopt if the reconstructed tuple is deleted.
inline auto ReconstructTuple(const TabMeta *schema, const RmRecord &base_tuple, const TupleMeta &base_meta,
                             const std::vector<UndoLog> &undo_logs) -> std::optional<RmRecord> {
    RmRecord result(base_tuple.size);
    memcpy(result.data, base_tuple.data, base_tuple.size);

    // Apply undo logs from last to first (reverse chronological)
    for (auto it = undo_logs.rbegin(); it != undo_logs.rend(); ++it) {
        const auto &log = *it;
        if (log.is_deleted_) {
            return std::nullopt;  // Tuple was deleted in this version
        }
        // Apply modified fields from the undo log
        for (size_t i = 0; i < log.modified_fields_.size() && i < log.tuple_.size(); i++) {
            if (log.modified_fields_[i]) {
                const auto &col = schema->cols[i];
                log.tuple_[i].init_raw(col.len);
                memcpy(result.data + col.offset, log.tuple_[i].raw->data, col.len);
            }
        }
    }
    return result;
}
