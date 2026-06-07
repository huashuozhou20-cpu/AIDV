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
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "disk_manager.h"
#include "errors.h"
#include "page.h"
#include "replacer/lru_replacer.h"
#include "replacer/replacer.h"

// Number of independent shards.  Each shard owns a private latch, LRU
// replacer, page table and free list — eliminating the global-lock
// bottleneck under mixed read/write workloads.
static constexpr size_t BPM_SHARD_COUNT = 16;

/**
 * BufferPoolShard — one independent partition of the buffer pool.
 */
class BufferPoolShard {
   public:
    BufferPoolShard(size_t pool_size, DiskManager *disk_manager,
                    Page *pages_start);

    Page* fetch_page(PageId page_id);
    bool  unpin_page(PageId page_id, bool is_dirty);
    bool  flush_page(PageId page_id);
    Page* new_page(PageId* page_id);   // page_id->page_no MUST already be set
    bool  delete_page(PageId page_id);
    void  flush_all_pages(int fd);
    void  discard_pages(int fd);  // evict all pages for fd without disk deallocation

    std::vector<PageId> collect_dirty_pages();
    void flush_dirty_pages();

   private:
    size_t pool_size_;
    Page*  pages_;          // pointer into the global pages_ array slice
    std::unordered_map<PageId, frame_id_t, PageIdHash> page_table_;
    std::list<frame_id_t> free_list_;
    LRUReplacer replacer_;
    std::mutex  latch_;
    DiskManager* disk_manager_;

    bool find_victim_page(frame_id_t* frame_id);
    void update_page(Page* page, PageId new_page_id, frame_id_t new_frame_id);
};

/* ------------------------------------------------------------------ */

class BufferPoolManager {
   public:
    BufferPoolManager(size_t pool_size, DiskManager* disk_manager);
    ~BufferPoolManager();

    static void mark_dirty(Page* page) { page->is_dirty_ = true; }

    Page* fetch_page(PageId page_id);
    bool  unpin_page(PageId page_id, bool is_dirty);
    bool  flush_page(PageId page_id);
    Page* new_page(PageId* page_id);
    bool  delete_page(PageId page_id);
    void  flush_all_pages(int fd);
    void  discard_pages(int fd);  // evict all pages for fd without disk deallocation

    // Background flush thread
    void start_flush_thread();
    void stop_flush_thread();

   private:
    size_t shard_of(PageId page_id) const {
        // Stable routing by page_no — distributes pages of a file across shards.
        return static_cast<size_t>(page_id.page_no) % shards_.size();
    }

    size_t pool_size_;
    Page*  pages_;
    DiskManager* disk_manager_;
    std::vector<std::unique_ptr<BufferPoolShard>> shards_;

    void flush_thread_loop();
    std::thread flush_thread_;
    std::atomic<bool> stop_flush_{false};
    std::condition_variable flush_cv_;
    std::mutex flush_mutex_;
    std::chrono::milliseconds flush_interval_{1000};
};