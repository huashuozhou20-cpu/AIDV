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

// ============================================================================
// EpollServer — edge-triggered epoll Reactor with worker thread pool.
//
// Replaces the per-connection-thread model so the I/O thread never blocks on
// individual client reads/writes.  Complete SQL statements (terminated by ';')
// are dispatched to a fixed-size worker pool for execution; computed results
// are written to per-connection output buffers and sent asynchronously by the
// I/O thread when the socket is write-ready.
// ============================================================================

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

static constexpr int    MAX_EVENTS   = 256;
static constexpr int    MAX_CONN     = 128;
static constexpr size_t READ_CHUNK   = 65536;

// ---- Per-connection state --------------------------------------------------
struct Connection {
    int         fd;
    std::string read_buf;        // accumulated input until ';'
    std::string write_buf;       // pending output
    size_t      write_off = 0;   // bytes already sent from write_buf
    int64_t     txn_id    = -1;  // INVALID_TXN_ID
    bool        closing   = false;
    bool        busy      = false;  // a query is being processed for this conn

    // Outbound buffer lock — worker writes result, I/O thread reads.
    std::mutex  out_mutex_;

    bool has_pending_write() const {
        return write_off < write_buf.size();
    }
};

// ---- SQL processing task ---------------------------------------------------
struct SqlTask {
    int         conn_fd;
    std::string sql;
};

// ---- Worker thread pool ----------------------------------------------------
class WorkerPool {
   public:
    using Processor = std::function<void(int conn_fd, const std::string& sql)>;

    explicit WorkerPool(int num_workers, Processor proc);
    ~WorkerPool();

    void submit(int conn_fd, std::string sql);
    void shutdown();

   private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<SqlTask>      queue_;
    std::mutex               mutex_;
    std::condition_variable  cv_;
    std::atomic<bool>        stop_{false};
    Processor                process_;
};

// ---- Epoll Reactor server --------------------------------------------------
class EpollServer {
   public:
    // |process_fn| is called by a worker thread to execute one SQL statement.
    // Its signature:  void(int conn_fd, const std::string& sql)
    EpollServer(int port, std::function<void(int, const std::string&)> process_fn,
                int worker_threads = 4);

    ~EpollServer();

    // Blocking call — runs the event loop until stop() is called.
    void run();

    // Signal-safe — may be called from a signal handler.
    void stop();

    // Append result data to a connection's output buffer (thread-safe).
    // Called by worker threads after processing a query.
    void enqueue_response(int conn_fd, const char* data, size_t len);

   private:
    void make_nonblocking(int fd);
    void add_epoll(int fd, uint32_t events);
    void mod_epoll(int fd, uint32_t events);
    void handle_accept();
    void handle_read(int fd);
    void handle_write(int fd);
    void close_connection(int fd);

    int          listen_fd_;
    int          epoll_fd_;
    int          notify_fd_;    // eventfd: workers wake I/O thread for writes
    std::atomic<bool> running_{false};
    int          port_;

    std::mutex                      conn_mutex_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;

    std::unique_ptr<WorkerPool>     pool_;
    std::function<void(int, const std::string&)> process_fn_;
};
