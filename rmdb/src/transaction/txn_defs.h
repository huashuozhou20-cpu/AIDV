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

#include "common/config.h"
#include "defs.h"
#include "record/rm_defs.h"

/* 标识事务状态 */
enum class TransactionState { DEFAULT, GROWING, SHRINKING, COMMITTED, ABORTED };

/* 系统的隔离级别，当前赛题中为可串行化隔离级别 */
enum class IsolationLevel { READ_UNCOMMITTED, REPEATABLE_READ, READ_COMMITTED, SERIALIZABLE };

/* 事务写操作类型，包括插入、删除、更新三种操作 */
enum class WType { INSERT_TUPLE = 0, DELETE_TUPLE, UPDATE_TUPLE};

/**
 * @brief 事务的写操作记录，用于事务的回滚
 * INSERT
 * --------------------------------
 * | wtype | tab_name | tuple_rid |
 * --------------------------------
 * DELETE / UPDATE
 * ----------------------------------------------
 * | wtype | tab_name | tuple_rid | tuple_value |
 * ----------------------------------------------
 */
class WriteRecord {
   public:
    WriteRecord() = default;

    // constructor for insert operation
    WriteRecord(WType wtype, const std::string &tab_name, const Rid &rid)
        : wtype_(wtype), tab_name_(tab_name), rid_(rid) {}

    // constructor for delete & update operation
    WriteRecord(WType wtype, const std::string &tab_name, const Rid &rid, const RmRecord &record)
        : wtype_(wtype), tab_name_(tab_name), rid_(rid), record_(record) {}

    ~WriteRecord() = default;

    inline RmRecord &GetRecord() { return record_; }

    inline Rid &GetRid() { return rid_; }

    inline WType &GetWriteType() { return wtype_; }

    inline std::string &GetTableName() { return tab_name_; }

   private:
    WType wtype_;
    std::string tab_name_;
    Rid rid_;
    RmRecord record_;
};

/* 多粒度锁，加锁对象的类型，包括记录、表和索引键间隙 */
enum class LockDataType { TABLE = 0, RECORD = 1, GAP = 2 };

/**
 * @description: 加锁对象的唯一标识
 *
 * 支持三种粒度的锁标识：
 *   1. TABLE:      由 fd_ 标识
 *   2. RECORD:     由 (fd_, rid_) 标识
 *   3. GAP/INDEX:  由 (fd_, index_id_, key_data_) 标识
 *
 * GAP 锁使用索引逻辑键值而非物理 RID，因此不同物理位置
 * 但落在同一逻辑键区间的 INSERT 会与 GAP 锁在锁表中碰撞。
 */
class LockDataId {
   public:
    /* 表级锁 */
    LockDataId(int fd, LockDataType type) {
        assert(type == LockDataType::TABLE);
        fd_ = fd;
        type_ = type;
        rid_.page_no = -1;
        rid_.slot_no = -1;
        index_id_ = -1;
    }

    /* 行级锁（基于物理 RID） */
    LockDataId(int fd, const Rid &rid, LockDataType type) {
        assert(type == LockDataType::RECORD);
        fd_ = fd;
        rid_ = rid;
        type_ = type;
        index_id_ = -1;
    }

    /* 索引键间隙锁（基于逻辑 Index Key） */
    LockDataId(int fd, int index_id, const char *key_data, int key_len, LockDataType type) {
        assert(type == LockDataType::GAP);
        fd_ = fd;
        type_ = type;
        index_id_ = index_id;
        key_data_.assign(key_data, key_len);
        rid_.page_no = -1;
        rid_.slot_no = -1;
    }

    inline int64_t Get() const {
        if (type_ == LockDataType::TABLE) {
            return static_cast<int64_t>(fd_);
        } else if (type_ == LockDataType::RECORD) {
            return ((static_cast<int64_t>(type_)) << 63) | ((static_cast<int64_t>(fd_)) << 31) |
                   ((static_cast<int64_t>(rid_.page_no)) << 16) | rid_.slot_no;
        } else {
            // GAP: hash (fd_, index_id_, key_data_) into int64_t
            int64_t h = static_cast<int64_t>(type_) << 56;
            h ^= (static_cast<int64_t>(fd_) << 40);
            h ^= (static_cast<int64_t>(index_id_) << 32);
            for (size_t i = 0; i < key_data_.size(); i++) {
                h ^= (static_cast<int64_t>(static_cast<unsigned char>(key_data_[i]))
                      << (8 * (i % 4)));
            }
            return h;
        }
    }

    bool operator==(const LockDataId &other) const {
        if (type_ != other.type_) return false;
        if (fd_ != other.fd_) return false;
        if (type_ == LockDataType::TABLE) return true;
        if (type_ == LockDataType::RECORD) return rid_ == other.rid_;
        // GAP: compare index_id and key data
        return index_id_ == other.index_id_ && key_data_ == other.key_data_;
    }
    int fd_;
    Rid rid_;
    int index_id_ = -1;
    std::string key_data_;
    LockDataType type_;
};

template <>
struct std::hash<LockDataId> {
    size_t operator()(const LockDataId &obj) const { return std::hash<int64_t>()(obj.Get()); }
};

/* 事务回滚原因 */
enum class AbortReason { LOCK_ON_SHIRINKING = 0, UPGRADE_CONFLICT, DEADLOCK_PREVENTION };

/* 事务回滚异常，在rmdb.cpp中进行处理 */
class TransactionAbortException : public std::exception {
    txn_id_t txn_id_;
    AbortReason abort_reason_;

   public:
    explicit TransactionAbortException(txn_id_t txn_id, AbortReason abort_reason)
        : txn_id_(txn_id), abort_reason_(abort_reason) {}

    txn_id_t get_transaction_id() { return txn_id_; }
    AbortReason GetAbortReason() { return abort_reason_; }
    std::string GetInfo() {
        switch (abort_reason_) {
            case AbortReason::LOCK_ON_SHIRINKING: {
                return "Transaction " + std::to_string(txn_id_) +
                       " aborted because it cannot request locks on SHRINKING phase\n";
            } break;

            case AbortReason::UPGRADE_CONFLICT: {
                return "Transaction " + std::to_string(txn_id_) +
                       " aborted because another transaction is waiting for upgrading\n";
            } break;

            case AbortReason::DEADLOCK_PREVENTION: {
                return "Transaction " + std::to_string(txn_id_) + " aborted for deadlock prevention\n";
            } break;

            default: {
                return "Transaction aborted\n";
            } break;
        }
    }
};