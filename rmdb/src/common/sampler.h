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
// Built-in statistical CPU profiler for environments without perf / gdb.
// Uses timer_create + SIGPROF to periodically capture raw backtrace
// addresses into a file.  Post-processing resolves symbols via addr2line
// and renders a FlameGraph SVG.
//
// Usage:
//   Sampler::start("/tmp/rmdb_samples.bin");
//   ... benchmark ...
//   Sampler::stop();
//
//   Then:  python3 tools/fold_stacks.py /tmp/rmdb_samples.bin
//             ./build/bin/rmdb | FlameGraph/flamegraph.pl > database_perf.svg
// ============================================================================

#include <atomic>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <execinfo.h>

class Sampler {
   public:
    static constexpr int MAX_DEPTH = 32;
    static constexpr long INTERVAL_NS = 10'000'000L;  // 10 ms → 100 Hz

    // Start sampling on all threads.  Samples are raw void* arrays written
    // to |path|; each record is [count:u16][depth:u16][addrs:depth×void*].
    static void start(const char* path) {
        if (running_.exchange(true)) return;

        fd_ = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_ < 0) { running_ = false; return; }

        struct sigaction sa{};
        sa.sa_flags     = SA_SIGINFO | SA_RESTART;
        sa.sa_sigaction = handler;
        ::sigaction(SIGPROF, &sa, nullptr);

        struct sigevent sev{};
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo  = SIGPROF;
        ::timer_create(CLOCK_MONOTONIC, &sev, &timer_id_);

        struct itimerspec its{};
        its.it_value.tv_sec    = 0;
        its.it_value.tv_nsec   = INTERVAL_NS;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = INTERVAL_NS;
        ::timer_settime(timer_id_, 0, &its, nullptr);

        ::fprintf(stderr, "[sampler] started → %s  (100 Hz)\n", path);
    }

    static void stop() {
        if (!running_.exchange(false)) return;

        ::timer_delete(timer_id_);
        ::signal(SIGPROF, SIG_DFL);
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
        ::fprintf(stderr, "[sampler] stopped  (samples: %d)\n",
                  sample_count_.load());
    }

   private:
    static inline std::atomic<bool> running_{false};
    static inline std::atomic<int>  sample_count_{0};
    static inline timer_t timer_id_{};
    static inline int     fd_ = -1;

    // Async-signal-safe: backtrace reads memory, write is a syscall.
    // We store raw addresses; symbol resolution happens offline.
    static void handler(int /*sig*/, siginfo_t* /*si*/, void* /*uc*/) {
        void* buffer[MAX_DEPTH];
        int n = ::backtrace(buffer, MAX_DEPTH);
        if (n <= 2 || fd_ < 0) return;

        // Skip the two innermost frames (signal trampoline + this handler)
        void** frames = buffer + 2;
        int     depth = n - 2;

        uint16_t hdr[2];
        hdr[0] = static_cast<uint16_t>(sample_count_.fetch_add(1) + 1);
        hdr[1] = static_cast<uint16_t>(depth);
        (void)::write(fd_, hdr, sizeof(hdr));
        (void)::write(fd_, frames, static_cast<size_t>(depth) * sizeof(void*));
    }
};
