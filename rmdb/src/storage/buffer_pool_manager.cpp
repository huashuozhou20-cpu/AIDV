/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "buffer_pool_manager.h"

// ============================================================================
// BufferPoolShard — identical logic to the original BufferPoolManager, but
//                  operating on only a slice of the global pages_ array.
// ============================================================================

BufferPoolShard::BufferPoolShard(size_t pool_size, DiskManager *disk_manager,
                                 Page *pages_start)
    : pool_size_(pool_size), pages_(pages_start),
      replacer_(pool_size), disk_manager_(disk_manager)
{
    for (size_t i = 0; i < pool_size_; ++i) {
        free_list_.emplace_back(static_cast<frame_id_t>(i));
    }
}

bool BufferPoolShard::find_victim_page(frame_id_t *frame_id) {
    if (!free_list_.empty()) {
        *frame_id = free_list_.front();
        free_list_.pop_front();
        return true;
    }
    return replacer_.victim(frame_id);
}

void BufferPoolShard::update_page(Page *page, PageId new_page_id,
                                   frame_id_t new_frame_id) {
    // Must never evict a page that is still pinned by an active iterator or executor.
    assert(page->pin_count_ == 0);
    if (page->is_dirty_) {
        disk_manager_->write_page(page->id_.fd, page->id_.page_no,
                                  page->data_, PAGE_SIZE);
        page->is_dirty_ = false;
    }
    page_table_.erase(page->id_);
    page->reset_memory();
    page->id_ = new_page_id;
    page_table_[new_page_id] = new_frame_id;
}

Page *BufferPoolShard::fetch_page(PageId page_id) {
    std::scoped_lock lock{latch_};
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        frame_id_t frame_id = it->second;
        Page *page = &pages_[frame_id];
        page->pin_count_++;
        replacer_.pin(frame_id);
        return page;
    }
    frame_id_t frame_id;
    if (!find_victim_page(&frame_id)) {
        return nullptr;
    }
    Page *page = &pages_[frame_id];
    update_page(page, page_id, frame_id);
    disk_manager_->read_page(page_id.fd, page_id.page_no, page->data_, PAGE_SIZE);
    page->pin_count_ = 1;
    replacer_.pin(frame_id);
    return page;
}

bool BufferPoolShard::unpin_page(PageId page_id, bool is_dirty) {
    std::scoped_lock lock{latch_};
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }
    Page *page = &pages_[it->second];
    if (page->pin_count_ <= 0) {
        return false;
    }
    page->pin_count_--;
    if (page->pin_count_ == 0) {
        replacer_.unpin(it->second);
    }
    if (is_dirty) {
        page->is_dirty_ = true;
    }
    return true;
}

bool BufferPoolShard::flush_page(PageId page_id) {
    std::scoped_lock lock{latch_};
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }
    Page *page = &pages_[it->second];
    disk_manager_->write_page(page->id_.fd, page->id_.page_no,
                              page->data_, PAGE_SIZE);
    page->is_dirty_ = false;
    return true;
}

Page *BufferPoolShard::new_page(PageId *page_id) {
    // page_id->page_no MUST already be set by the caller (global allocation).
    // This shard only manages the buffer frame.
    std::scoped_lock lock{latch_};

    frame_id_t frame_id;
    if (!find_victim_page(&frame_id)) {
        return nullptr;
    }
    Page *page = &pages_[frame_id];
    // Victim page must not be pinned by any active thread.
    assert(page->pin_count_ == 0);
    if (page->is_dirty_) {
        disk_manager_->write_page(page->id_.fd, page->id_.page_no,
                                  page->data_, PAGE_SIZE);
        page->is_dirty_ = false;
    }
    page_table_.erase(page->id_);
    page->reset_memory();
    page->id_ = *page_id;
    page_table_[*page_id] = frame_id;
    page->pin_count_ = 1;
    replacer_.pin(frame_id);
    return page;
}

bool BufferPoolShard::delete_page(PageId page_id) {
    std::scoped_lock lock{latch_};
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true;
    }
    frame_id_t frame_id = it->second;
    Page *page = &pages_[frame_id];
    if (page->pin_count_ > 0) {
        return false;
    }
    if (page->is_dirty_) {
        disk_manager_->write_page(page->id_.fd, page->id_.page_no,
                                  page->data_, PAGE_SIZE);
    }
    replacer_.pin(frame_id);
    disk_manager_->deallocate_page(page->id_.fd, page->id_.page_no);
    page_table_.erase(it);
    page->reset_memory();
    page->id_ = {.fd = INVALID_PAGE_ID, .page_no = INVALID_PAGE_ID};
    page->is_dirty_ = false;
    page->pin_count_ = 0;
    free_list_.push_back(frame_id);
    return true;
}

void BufferPoolShard::flush_all_pages(int fd) {
    std::scoped_lock lock{latch_};
    for (auto &[page_id, frame_id] : page_table_) {
        if (page_id.fd == fd) {
            Page *page = &pages_[frame_id];
            disk_manager_->write_page(page->id_.fd, page->id_.page_no,
                                      page->data_, PAGE_SIZE);
            page->is_dirty_ = false;
        }
    }
}

void BufferPoolShard::discard_pages(int fd) {
    // Collect page_ids for this fd, then use existing delete_page for each.
    // Must NOT hold latch_ while calling delete_page (it acquires latch_).
    std::vector<PageId> to_delete;
    {
        std::scoped_lock lock{latch_};
        for (auto &[page_id, frame_id] : page_table_) {
            if (page_id.fd == fd) to_delete.push_back(page_id);
        }
    }
    for (auto &page_id : to_delete) {
        delete_page(page_id);  // safe: delete_page acquires its own latch
    }
}

std::vector<PageId> BufferPoolShard::collect_dirty_pages() {
    std::scoped_lock lock{latch_};
    std::vector<PageId> dirty;
    for (auto &[page_id, frame_id] : page_table_) {
        Page *page = &pages_[frame_id];
        if (page->is_dirty_ && page->pin_count_ == 0) {
            dirty.push_back(page_id);
        }
    }
    return dirty;
}

void BufferPoolShard::flush_dirty_pages() {
    auto dirty = collect_dirty_pages();
    for (auto &page_id : dirty) {
        flush_page(page_id);
    }
}

// ============================================================================
// BufferPoolManager — thin router that maps page_no → shard
// ============================================================================

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager)
{
    pages_ = new Page[pool_size_];

    // Use fewer shards when pool is tiny (e.g., unit tests with 10 pages).
    // In production (256 MB, 65536 pages) this equals BPM_SHARD_COUNT.
    // Ensure at least 4 pages per shard for reasonable LRU behaviour.
    size_t shard_cnt = std::max(size_t(1), std::min(BPM_SHARD_COUNT, pool_size_ / 4));
    size_t per_shard = pool_size_ / shard_cnt;
    size_t remainder = pool_size_ % shard_cnt;

    size_t offset = 0;
    for (size_t i = 0; i < shard_cnt; ++i) {
        size_t shard_pool_size = per_shard + (i < remainder ? 1 : 0);
        shards_.emplace_back(std::make_unique<BufferPoolShard>(
            shard_pool_size, disk_manager_, pages_ + offset));
        offset += shard_pool_size;
    }

    start_flush_thread();
}

BufferPoolManager::~BufferPoolManager() {
    stop_flush_thread();
    delete[] pages_;
}

Page *BufferPoolManager::fetch_page(PageId page_id) {
    return shards_[shard_of(page_id)]->fetch_page(page_id);
}

bool BufferPoolManager::unpin_page(PageId page_id, bool is_dirty) {
    return shards_[shard_of(page_id)]->unpin_page(page_id, is_dirty);
}

bool BufferPoolManager::flush_page(PageId page_id) {
    return shards_[shard_of(page_id)]->flush_page(page_id);
}

Page *BufferPoolManager::new_page(PageId *page_id) {
    // Global allocation: use DiskManager's atomic counter to get a page_no,
    // then route to the shard that owns that page_no % SHARD_COUNT.
    int fd = page_id->fd;
    page_id->page_no = disk_manager_->allocate_page(fd);
    return shards_[shard_of(*page_id)]->new_page(page_id);
}

bool BufferPoolManager::delete_page(PageId page_id) {
    return shards_[shard_of(page_id)]->delete_page(page_id);
}

void BufferPoolManager::flush_all_pages(int fd) {
    for (auto &shard : shards_) {
        shard->flush_all_pages(fd);
    }
}

void BufferPoolManager::discard_pages(int fd) {
    for (auto &shard : shards_) {
        shard->discard_pages(fd);
    }
}

// ---------------------------------------------------------------------------
// Background flush — iterate all shards independently
// ---------------------------------------------------------------------------

void BufferPoolManager::flush_thread_loop() {
    while (!stop_flush_) {
        {
            std::unique_lock<std::mutex> lock(flush_mutex_);
            flush_cv_.wait_for(lock, flush_interval_, [this] {
                return stop_flush_.load();
            });
        }
        if (stop_flush_) break;

        for (auto &shard : shards_) {
            shard->flush_dirty_pages();
        }
    }
}

void BufferPoolManager::start_flush_thread() {
    if (flush_thread_.joinable()) return;
    stop_flush_ = false;
    flush_thread_ = std::thread(&BufferPoolManager::flush_thread_loop, this);
}

void BufferPoolManager::stop_flush_thread() {
    stop_flush_ = true;
    flush_cv_.notify_all();
    if (flush_thread_.joinable()) {
        flush_thread_.join();
    }
}