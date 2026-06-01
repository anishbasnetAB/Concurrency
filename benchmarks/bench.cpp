// Unified benchmark suite — compares all structures against their
// mutex-based equivalents. Run with -O2 for meaningful numbers.
//
// Build via CMake: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
// Run: ./build/bench

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <mutex>
#include <stack>
#include <queue>
#include <functional>
#include <atomic>
#include <vector>
#include <string>
#include <algorithm>

#include "spsc_queue.h"
#include "lock_free_stack.h"
#include "thread_pool.h"

using Clock = std::chrono::high_resolution_clock;

static long long elapsed_ns(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - start).count();
}

static void header(const std::string& s) {
    std::cout << "\n" << s << "\n" << std::string(s.size(), '-') << "\n";
}

static void row(const std::string& label, long long ns, long long ops) {
    double ns_per_op = static_cast<double>(ns) / ops;
    double mops      = ops / (ns / 1e9) / 1e6;
    std::cout << std::left  << std::setw(22) << label
              << std::right << std::setw(7)  << std::fixed << std::setprecision(1)
              << ns_per_op  << " ns/op"
              << "   " << std::setprecision(1) << mops << " Mops/s\n";
}

// ── 1. SPSC Queue vs Mutex Queue ────────────────────────────────────────────

void bench_queue(int ops) {
    header("Queue: SPSC  vs  Mutex  (" + std::to_string(ops / 1'000'000) + "M ops, 2 threads)");

    // SPSC (lock-free)
    {
        SPSCQueue<int, 4096> q;
        auto t0 = Clock::now();
        std::thread prod([&] {
            for (int i = 0; i < ops; ++i)
                while (!q.push(i));
        });
        std::thread cons([&] {
            int v;
            for (int i = 0; i < ops; ++i)
                while (!q.pop(v));
        });
        prod.join(); cons.join();
        row("SPSC Queue (lock-free)", elapsed_ns(t0), ops);
    }

    // Mutex queue (same 1-producer / 1-consumer topology)
    {
        std::queue<int> q;
        std::mutex mu;
        auto t0 = Clock::now();
        std::thread prod([&] {
            for (int i = 0; i < ops; ++i) {
                std::lock_guard lock(mu);
                q.push(i);
            }
        });
        std::thread cons([&] {
            int v; int done = 0;
            while (done < ops) {
                std::unique_lock lock(mu);
                if (!q.empty()) { v = q.front(); q.pop(); ++done; }
            }
        });
        prod.join(); cons.join();
        row("Mutex Queue", elapsed_ns(t0), ops);
    }
}

// ── 2. Lock-Free Stack vs Mutex Stack ─────────────────────────────────────────
// Note: single-threaded benchmarks favour mutex — uncontended lock acquisition
// is a handful of instructions and hits L1 cache. Lock-free wins when multiple
// threads contend; this benchmark shows the baseline per-operation cost.

void bench_stack(int ops) {
    header("Stack: Lock-Free  vs  Mutex  (" + std::to_string(ops / 1000) +
           "K push+pop, single thread — contention-free baseline)");

    // Lock-free
    {
        LockFreeStack<int> s;
        auto t0 = Clock::now();
        for (int i = 0; i < ops; ++i) s.push(i);
        int v;
        for (int i = 0; i < ops; ++i) s.pop(v);
        row("Lock-Free Stack", elapsed_ns(t0), ops * 2);
    }

    // Mutex stack
    {
        std::stack<int> s;
        std::mutex mu;
        auto t0 = Clock::now();
        for (int i = 0; i < ops; ++i) {
            std::lock_guard lock(mu);
            s.push(i);
        }
        int v;
        for (int i = 0; i < ops; ++i) {
            std::lock_guard lock(mu);
            v = s.top(); s.pop();
        }
        (void)v;
        row("Mutex Stack", elapsed_ns(t0), ops * 2);
    }
}

// ── 3. Thread Pool Throughput ─────────────────────────────────────────────────
// Two sub-benchmarks:
//   Light  (~100 ns/task): scheduling overhead > task — throughput degrades with more workers.
//   Heavy (~100 µs/task):  task >> overhead  — scales linearly with cores.

static void pool_run(int tasks, int work_iters, size_t nw) {
    ThreadPool pool(nw);
    std::atomic<int> done{0};
    auto t0 = Clock::now();
    for (int i = 0; i < tasks; ++i) {
        pool.enqueue([&done, work_iters] {
            volatile long long acc = 0;
            for (int j = 0; j < work_iters; ++j) acc += j;
            done.fetch_add(1, std::memory_order_release);
        });
    }
    while (done.load(std::memory_order_acquire) < tasks);
    long long ns = elapsed_ns(t0);
    std::cout << std::left  << std::setw(22) << (std::to_string(nw) + " worker(s)")
              << std::right << std::setw(7)  << ns / 1'000'000 << " ms"
              << "   "      << std::setw(10) << tasks * 1'000'000'000LL / ns << " tasks/s\n";
}

void bench_thread_pool() {
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());

    header("Thread Pool — Light tasks (~100 ns each, 20K tasks)\n"
           "  Scheduling overhead dominates — more workers hurts");
    for (size_t n : {1u, 2u, 4u, (unsigned)hw})
        pool_run(20'000, 100, n);

    header("Thread Pool — Heavy tasks (~100 µs each, 400 tasks)\n"
           "  Task >> overhead — throughput scales with cores");
    for (size_t n : {1u, 2u, 4u, (unsigned)hw})
        pool_run(400, 100'000, n);
}

// ── 4. Memory Order Cost ──────────────────────────────────────────────────────

void bench_memory_order(int ops) {
    header("Atomic fetch_add Cost  (" + std::to_string(ops / 1'000'000) + "M ops, single thread)");

    // fetch_add is a read-modify-write — cannot be elided by the optimizer.
    // On x86 all orders use LOCK XADD, so cost is the same.
    // On ARM, relaxed = ldxr/stxr loop, seq_cst adds a DMB barrier.
    auto bench = [&](const char* name, auto order) {
        std::atomic<long long> a{0};
        auto t0 = Clock::now();
        for (int i = 0; i < ops; ++i)
            a.fetch_add(1, order);
        row(name, elapsed_ns(t0), ops);
    };

    bench("relaxed", std::memory_order_relaxed);
    bench("acq_rel", std::memory_order_acq_rel);
    bench("seq_cst", std::memory_order_seq_cst);
}

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "Concurrency Benchmark Suite\n";
    std::cout << "Hardware threads: " << std::thread::hardware_concurrency() << "\n";
    std::cout << "Build type: Release (-O2)\n";

    bench_queue(10'000'000);
    bench_stack(1'000'000);
    bench_thread_pool();
    bench_memory_order(20'000'000);

    std::cout << "\n";
    return 0;
}
