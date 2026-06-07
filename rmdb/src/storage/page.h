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

#include "common/rwlatch.h"
#include "common/config.h"

/**
 * @description: 存储层每个Page的id的声明
 */
struct PageId {
    int fd;  //  Page所在的磁盘文件开启后的文件描述符, 来定位打开的文件在内存中的位置
    page_id_t page_no = INVALID_PAGE_ID;

    friend bool operator==(const PageId &x, const PageId &y) { return x.fd == y.fd && x.page_no == y.page_no; }
    bool operator<(const PageId& x) const {
        if(fd < x.fd) return true;
        return page_no < x.page_no;
    }

    std::string toString() {
        return "{fd: " + std::to_string(fd) + " page_no: " + std::to_string(page_no) + "}";
    }

    inline int64_t Get() const {
        return (static_cast<int64_t>(fd << 16) | page_no);
    }
};

// PageId的自定义哈希算法, 用于构建unordered_map<PageId, frame_id_t, PageIdHash>
struct PageIdHash {
    size_t operator()(const PageId &x) const { return (x.fd << 16) | x.page_no; }
};

template <>
struct std::hash<PageId> {
    size_t operator()(const PageId &obj) const { return std::hash<int64_t>()(obj.Get()); }
};

/**
 * @description: Page类声明, Page是RMDB数据块的单位、是负责数据操作Record模块的操作对象，
 * Page对象在磁盘上有文件存储, 若在Buffer中则有帧偏移, 并非特指Buffer或Disk上的数据
 */
class Page {
    friend class BufferPoolManager;
    friend class BufferPoolShard;

   public:

    Page() { reset_memory(); }

    ~Page() = default;

    PageId get_page_id() const { return id_; }

    inline char *get_data() { return data_; }

    bool is_dirty() const { return is_dirty_; }

    static constexpr size_t OFFSET_PAGE_START = 0;
    static constexpr size_t OFFSET_LSN = 0;
    static constexpr size_t OFFSET_PAGE_HDR = 4;

    inline lsn_t get_page_lsn() { return *reinterpret_cast<lsn_t *>(get_data() + OFFSET_LSN) ; }

    inline void set_page_lsn(lsn_t page_lsn) { memcpy(get_data() + OFFSET_LSN, &page_lsn, sizeof(lsn_t)); }

    // ---- B+Tree latch crabbing ----
    // Shared (read) lock: used when traversing/searching without modifying
    inline void rlock() const { latch_.lock_shared(); }
    inline void runlock() const { latch_.unlock_shared(); }

    // Exclusive (write) lock: used when inserting/deleting/splitting
    inline void wlock() { latch_.lock(); }
    inline void wunlock() { latch_.unlock(); }

    // Access the underlying latch (for optimistic sequence-based traversal)
    inline ReaderWriterSpinLatch &GetLatch() const { return latch_; }

   private:
    void reset_memory() { memset(data_, OFFSET_PAGE_START, PAGE_SIZE); }  // 将data_的PAGE_SIZE个字节填充为0

    /** page的唯一标识符 */
    PageId id_;

    /** The actual data that is stored within a page.
     *  该页面在bufferPool中的偏移地址
     */
    char data_[PAGE_SIZE] = {};

    /** 脏页判断 */
    bool is_dirty_ = false;

    /** The pin count of this page. */
    int pin_count_ = 0;

    /** Per-page read/write latch for B+Tree concurrency control (latch crabbing).
     *  Shared for readers, exclusive for writers.  User-space spinlock — no
     *  kernel involvement, pure CAS + cpu_relax back-off. */
    mutable ReaderWriterSpinLatch latch_;

   public:
    int get_pin_count() const { return pin_count_; }
};

/** Tag type for adopting an already-held page latch. */
struct adopt_latch_t {};
constexpr adopt_latch_t adopt_latch{};

/** RAII guard for Page read/write latches.
 *  Ensures the latch is released on scope exit, even if an exception is thrown.
 *  Supports two modes:
 *    - ACQUIRE (default): acquires the latch in the constructor.
 *    - ADOPT (with adopt_latch tag): takes ownership of an already-held latch;
 *      the destructor will still release it. */
class PageLatchGuard {
    Page *page_;
    bool exclusive_;

   public:
    /** Acquire a new latch on @p page. */
    PageLatchGuard(Page *page, bool exclusive) : page_(page), exclusive_(exclusive) {
        if (page_) {
            if (exclusive_)
                page_->wlock();
            else
                page_->rlock();
        }
    }

    /** Adopt an already-held latch. The destructor WILL release it. */
    PageLatchGuard(Page *page, bool exclusive, adopt_latch_t) : page_(page), exclusive_(exclusive) {}

    ~PageLatchGuard() { release(); }

    PageLatchGuard(const PageLatchGuard &) = delete;
    PageLatchGuard &operator=(const PageLatchGuard &) = delete;

    PageLatchGuard(PageLatchGuard &&other) noexcept : page_(other.page_), exclusive_(other.exclusive_) {
        other.page_ = nullptr;
    }

    PageLatchGuard &operator=(PageLatchGuard &&other) noexcept {
        if (this != &other) {
            release();
            page_ = other.page_;
            exclusive_ = other.exclusive_;
            other.page_ = nullptr;
        }
        return *this;
    }

    /** Release the latch early. */
    void release() {
        if (page_) {
            if (exclusive_)
                page_->wunlock();
            else
                page_->runlock();
            page_ = nullptr;
        }
    }

    /** Disarm without releasing — caller takes manual control. */
    void disarm() { page_ = nullptr; }
};