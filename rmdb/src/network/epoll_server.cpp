/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "epoll_server.h"

#include <cassert>
#include <cerrno>
#include <iostream>
#include <stdexcept>

// ============================================================================
// WorkerPool
// ============================================================================

WorkerPool::WorkerPool(int num_workers, Processor proc) : process_(std::move(proc)) {
    for (int i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&WorkerPool::worker_loop, this);
    }
}

WorkerPool::~WorkerPool() { shutdown(); }

void WorkerPool::submit(int conn_fd, std::string sql) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_.load()) return;
        queue_.push(SqlTask{conn_fd, std::move(sql)});
    }
    cv_.notify_one();
}

void WorkerPool::shutdown() {
    stop_ = true;
    cv_.notify_all();
    for (auto &t : workers_) {
        if (t.joinable()) t.join();
    }
}

void WorkerPool::worker_loop() {
    while (!stop_.load()) {
        SqlTask task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_.load() || !queue_.empty(); });
            if (stop_.load() && queue_.empty()) return;
            task = std::move(queue_.front());
            queue_.pop();
        }
        process_(task.conn_fd, task.sql);
    }
}

// ============================================================================
// EpollServer
// ============================================================================

EpollServer::EpollServer(int port,
                         std::function<void(int, const std::string&)> process_fn,
                         int worker_threads)
    : port_(port), process_fn_(std::move(process_fn)) {

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("socket() failed");

    int val = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
#ifdef SO_REUSEPORT
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
#endif

    make_nonblocking(listen_fd_);

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));
    if (::bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr),
               sizeof(addr)) < 0) {
        throw std::runtime_error("bind() failed");
    }
    if (::listen(listen_fd_, MAX_CONN) < 0) {
        throw std::runtime_error("listen() failed");
    }

    epoll_fd_ = ::epoll_create1(0);
    if (epoll_fd_ < 0) throw std::runtime_error("epoll_create1() failed");

    notify_fd_ = ::eventfd(0, EFD_NONBLOCK);
    if (notify_fd_ < 0) throw std::runtime_error("eventfd() failed");

    add_epoll(listen_fd_,   EPOLLIN | EPOLLET);
    add_epoll(notify_fd_,   EPOLLIN | EPOLLET);

    pool_ = std::make_unique<WorkerPool>(worker_threads, process_fn_);
}

EpollServer::~EpollServer() {
    stop();
    if (listen_fd_ >= 0) ::close(listen_fd_);
    if (epoll_fd_  >= 0) ::close(epoll_fd_);
    if (notify_fd_ >= 0) ::close(notify_fd_);
}

void EpollServer::run() {
    running_ = true;

    std::array<struct epoll_event, MAX_EVENTS> events{};

    while (running_.load()) {
        int nfds = ::epoll_wait(epoll_fd_, events.data(),
                                static_cast<int>(events.size()), -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;   // SIGPROF / signal-safe
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int    fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == listen_fd_) {
                // Accept in a loop (edge-triggered: must drain all)
                while (true) {
                    struct sockaddr_in client_addr{};
                    socklen_t clen = sizeof(client_addr);
                    int cfd = ::accept4(listen_fd_,
                                        reinterpret_cast<struct sockaddr*>(&client_addr),
                                        &clen, SOCK_NONBLOCK);
                    if (cfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        if (errno == EINTR) continue;
                        break;
                    }
                    if (connections_.size() >= MAX_CONN) {
                        ::close(cfd);
                        continue;
                    }
                    add_epoll(cfd, EPOLLIN | EPOLLET | EPOLLRDHUP);
                    {
                        std::lock_guard<std::mutex> lock(conn_mutex_);
                        connections_[cfd] = std::make_unique<Connection>();
                        connections_[cfd]->fd = cfd;
                    }
                }
            } else if (fd == notify_fd_) {
                // Drain eventfd — wake-up from worker threads
                uint64_t dummy;
                while (::read(notify_fd_, &dummy, sizeof(dummy)) > 0) {}
                // Re-enable EPOLLOUT for any connection with pending writes
                std::lock_guard<std::mutex> lock(conn_mutex_);
                for (auto &[cfd, conn] : connections_) {
                    if (conn && conn->has_pending_write()) {
                        mod_epoll(cfd, EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP);
                    }
                }
            } else {
                // Client socket event
                if (ev & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    close_connection(fd);
                } else {
                    if (ev & EPOLLIN)  handle_read(fd);
                    if (ev & EPOLLOUT) handle_write(fd);
                }
            }
        }
    }

    // Flush remaining writes before shutdown
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        for (auto &[fd, conn] : connections_) {
            if (conn && conn->has_pending_write()) {
                handle_write(fd);
            }
            ::close(fd);
        }
        connections_.clear();
    }
}

void EpollServer::stop() {
    running_ = false;
    // Wake epoll_wait by writing to notify_fd
    uint64_t one = 1;
    [[maybe_unused]] ssize_t _w = ::write(notify_fd_, &one, sizeof(one));
}

void EpollServer::enqueue_response(int conn_fd, const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    auto it = connections_.find(conn_fd);
    if (it == connections_.end() || !it->second) return;
    auto &conn = it->second;
    {
        std::lock_guard<std::mutex> out_lock(conn->out_mutex_);
        conn->write_buf.append(data, len);
    }

    // This worker is done — mark the connection ready for the next statement.
    conn->busy = false;

    // If there is another complete statement waiting in the read buffer,
    // dispatch it immediately without waiting for the next EPOLLIN.
    if (!conn->read_buf.empty()) {
        size_t semi = conn->read_buf.find(';');
        if (semi != std::string::npos) {
            std::string stmt = conn->read_buf.substr(0, semi + 1);
            conn->read_buf.erase(0, semi + 1);
            conn->busy = true;
            pool_->submit(conn_fd, std::move(stmt));
        }
    }

    // Wake I/O thread to send the pending output
    uint64_t one = 1;
    [[maybe_unused]] ssize_t _w = ::write(notify_fd_, &one, sizeof(one));
}

// ---- Private helpers -------------------------------------------------------

void EpollServer::make_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void EpollServer::add_epoll(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events   = events;
    ev.data.fd  = fd;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
}

void EpollServer::mod_epoll(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events   = events;
    ev.data.fd  = fd;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

void EpollServer::handle_read(int fd) {
    // Edge-triggered: must read until EAGAIN
    std::unique_ptr<Connection> *conn_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) return;
        conn_ptr = &it->second;
    }

    char buf[READ_CHUNK];
    bool dispatch = false;

    while (true) {
        ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            (*conn_ptr)->read_buf.append(buf, static_cast<size_t>(n));
            // Check each accumulated SQL statement (ends with ';')
            // Dispatch all complete statements
            dispatch = true;
        } else if (n == 0) {
            close_connection(fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            break;
        }
    }

    // Dispatch at most one complete statement per connection at a time.
    // Workers for the same connection must be serialized to avoid corrupting
    // shared transaction state (txn_id, Context).  After the worker finishes
    // it calls enqueue_response which will trigger another dispatch pass.
    if (dispatch && *conn_ptr && !(*conn_ptr)->busy) {
        auto &acc = (*conn_ptr)->read_buf;
        size_t semi = acc.find(';');
        if (semi != std::string::npos) {
            std::string stmt = acc.substr(0, semi + 1);
            acc.erase(0, semi + 1);
            (*conn_ptr)->busy = true;
            pool_->submit(fd, std::move(stmt));
        }
    }
}

void EpollServer::handle_write(int fd) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    auto it = connections_.find(fd);
    if (it == connections_.end() || !it->second) return;
    auto &conn = it->second;

    {
        std::lock_guard<std::mutex> out_lock(conn->out_mutex_);
        while (conn->write_off < conn->write_buf.size()) {
            const char *data = conn->write_buf.data() + conn->write_off;
            size_t      len  = conn->write_buf.size() - conn->write_off;
            ssize_t n = ::write(fd, data, len);
            if (n > 0) {
                conn->write_off += static_cast<size_t>(n);
            } else if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                break;
            }
        }
        // If fully written, compact the buffer
        if (conn->write_off >= conn->write_buf.size()) {
            conn->write_buf.clear();
            conn->write_off = 0;
        }
    }

    // Once done writing, switch back to read-only interest
    if (!conn->has_pending_write()) {
        mod_epoll(fd, EPOLLIN | EPOLLET | EPOLLRDHUP);
    }

    if (conn->closing && !conn->has_pending_write()) {
        ::close(fd);
        connections_.erase(fd);
    }
}

void EpollServer::close_connection(int fd) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;
    // Drain pending writes first, then close
    if (it->second && it->second->has_pending_write()) {
        it->second->closing = true;
        mod_epoll(fd, EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP);
    } else {
        ::close(fd);
        connections_.erase(fd);
    }
}
