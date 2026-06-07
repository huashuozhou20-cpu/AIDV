/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <cstring>
#include <immintrin.h>
#include "log_manager.h"

/**
 * @description: 添加日志记录到日志缓冲区中，并返回日志记录号。
 *               缓冲区满时拷贝到专用溢出缓冲区，释放锁后执行磁盘 I/O，
 *               避免持有 latch_ 期间做 fdatasync 阻塞其他线程追加日志。
 * @param {LogRecord*} log_record 要写入缓冲区的日志记录
 * @return {lsn_t} 返回该日志的日志记录号
 */
lsn_t LogManager::add_log_to_buffer(LogRecord* log_record) {
    std::unique_lock<std::mutex> lock(latch_);
    lsn_t lsn = global_lsn_++;
    log_record->lsn_ = lsn;
    int log_len = log_record->log_tot_len_;
    if (log_buffer_.is_full(log_len)) {
        if (log_buffer_.offset_ > 0) {
            // 拷贝到专用溢出缓冲区，释放 latch_ 后再做 I/O
            memcpy(overflow_buffer_, log_buffer_.buffer_, log_buffer_.offset_);
            int overflow_len = log_buffer_.offset_;
            log_buffer_.offset_ = 0;
            lock.unlock();
            {
                std::scoped_lock io_lock(io_latch_);
                disk_manager_->write_log(overflow_buffer_, overflow_len);
            }
            lock.lock();
        } else {
            log_buffer_.offset_ = 0;
        }
        persist_lsn_.store(lsn - 1, std::memory_order_release);
        persist_cv_.notify_all();
    }
    log_record->serialize(log_buffer_.buffer_ + log_buffer_.offset_);
    log_buffer_.offset_ += log_len;

    // Adaptive flush: wake background thread early when buffer is filling up
    if (log_buffer_.offset_ >= 65536) {  // 64 KB threshold
        flush_cv_.notify_one();
    }

    return lsn;
}

/**
 * @description: 把日志缓冲区的内容刷到磁盘中（Leader-Follower 组提交）
 * Leader 拷贝缓冲区后释放 latch_，在 I/O 期间允许并发追加日志；
 * Follower 在 group_commit_cv_ 上阻塞，Leader 完成 I/O 后唤醒它们。
 */
void LogManager::flush_log_to_disk() {
    std::unique_lock<std::mutex> lock(latch_);

    // Follower: 已有 Leader 在执行 I/O，阻塞等待
    if (is_flushing_) {
        group_commit_cv_.wait(lock);
        return;
    }

    // Leader: 无数据则直接返回
    if (log_buffer_.offset_ == 0) {
        return;
    }

    is_flushing_ = true;

    // 拷贝缓冲区数据到本地，重置 offset_，释放锁允许并发追加
    memcpy(flush_buffer_, log_buffer_.buffer_, log_buffer_.offset_);
    flush_offset_ = log_buffer_.offset_;
    log_buffer_.offset_ = 0;
    lsn_t flushed_up_to = global_lsn_ - 1;  // 捕获本次刷盘的 LSN 上界
    lock.unlock();

    // 核心 I/O：持有 io_latch_ 防止与 add_log_to_buffer 溢出刷盘竞态
    {
        std::scoped_lock io_lock(io_latch_);
        disk_manager_->write_log(flush_buffer_, flush_offset_);
    }

    // 重新获取锁，更新持久化位点，唤醒 Followers
    lock.lock();
    persist_lsn_.store(flushed_up_to, std::memory_order_release);
    is_flushing_ = false;
    group_commit_cv_.notify_all();
    persist_cv_.notify_all();
}

void LogManager::flush_thread_loop() {
    while (!stop_flush_) {
        {
            std::unique_lock<std::mutex> lock(flush_mutex_);
            flush_cv_.wait_for(lock, log_timeout, [this] {
                return stop_flush_.load();
            });
        }
        if (stop_flush_) break;
        flush_log_to_disk();
    }
}

void LogManager::start_flush_thread() {
    if (flush_thread_.joinable()) return;
    stop_flush_ = false;
    flush_thread_ = std::thread(&LogManager::flush_thread_loop, this);
}

void LogManager::wait_for_persist_lsn(lsn_t target) {
    // Fast path: spin briefly on the atomic persist_lsn_ (no lock needed).
    // If the background flusher just advanced it, we return without any
    // mutex or CV overhead.
    for (int spin = 0; spin < 100; ++spin) {
        if (persist_lsn_.load(std::memory_order_acquire) >= target) return;
        _mm_pause();
    }
    // Slow path: cv sleep.  The predicate re-checks under latch_ so a
    // notify_all between the spin and cv.wait is correctly handled.
    std::unique_lock<std::mutex> lock(latch_);
    persist_cv_.wait(lock, [this, target] {
        return persist_lsn_.load(std::memory_order_relaxed) >= target;
    });
}

void LogManager::stop_flush_thread() {
    stop_flush_ = true;
    flush_cv_.notify_all();
    if (flush_thread_.joinable()) {
        flush_thread_.join();
    }
}
