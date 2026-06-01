// Concrete examples of all three relevant C++ memory orders.
// Each example has a correctness assertion and a timing comparison.

#include <atomic>
#include <thread>
#include <cassert>
#include <chrono>
#include <iostream>
#include <iomanip>

using Clock = std::chrono::high_resolution_clock;

// ─────────────────────────────────────────────────────────
// 1. relaxed
//    Two threads each increment a counter 1M times.
//    fetch_add is atomic regardless of order — relaxed just
//    removes the inter-thread ordering fence, making it cheaper.
// ─────────────────────────────────────────────────────────
void example_relaxed() {
    std::atomic<int> counter{0};
    const int N = 1'000'000;

    auto bump = [&] {
        for (int i = 0; i < N; ++i)
            counter.fetch_add(1, std::memory_order_relaxed);
    };

    std::thread t1(bump), t2(bump);
    t1.join(); t2.join();

    assert(counter.load() == 2 * N);
    std::cout << "[relaxed]          counter = " << counter.load()
              << "  (expected " << 2 * N << ")  OK\n";
}

// ─────────────────────────────────────────────────────────
// 2. release / acquire
//    Producer writes payload, then sets a flag (release).
//    Consumer polls the flag (acquire), then reads payload.
//    The acquire-release pair forms a happens-before edge:
//    everything before the release store is visible after the
//    acquire load succeeds.
// ─────────────────────────────────────────────────────────
void example_release_acquire() {
    int payload = 0;
    std::atomic<bool> ready{false};

    std::thread producer([&] {
        payload = 42;
        ready.store(true, std::memory_order_release);
    });

    std::thread consumer([&] {
        while (!ready.load(std::memory_order_acquire));
        // payload is guaranteed to be 42 here.
        // Without release/acquire, the CPU's store buffer could
        // deliver ready=true before payload=42 reaches the consumer.
        assert(payload == 42);
    });

    producer.join(); consumer.join();
    std::cout << "[release/acquire]  payload = " << payload
              << "  (always 42)  OK\n";
}

// ─────────────────────────────────────────────────────────
// 3. seq_cst  (the default when no order is specified)
//    Guarantees a single total order across ALL seq_cst
//    operations on ALL atomic variables — stronger than
//    release/acquire which only orders pairs of variables.
//
//    Classic Dekker litmus test: without seq_cst, z could be 0
//    because t3 and t4 might observe x and y in different orders.
//    With seq_cst, either x-store or y-store comes first globally,
//    so at least one of t3/t4 must see both flags set.
// ─────────────────────────────────────────────────────────
void example_seq_cst() {
    std::atomic<bool> x{false}, y{false};
    std::atomic<int>  z{0};

    std::thread t1([&] { x.store(true); });           // seq_cst (default)
    std::thread t2([&] { y.store(true); });           // seq_cst (default)

    std::thread t3([&] {
        while (!x.load());
        if (y.load()) z.fetch_add(1);
    });

    std::thread t4([&] {
        while (!y.load());
        if (x.load()) z.fetch_add(1);
    });

    t1.join(); t2.join(); t3.join(); t4.join();

    // z is always 1 or 2, never 0.
    assert(z.load() >= 1);
    std::cout << "[seq_cst]          z = " << z.load()
              << "  (always >= 1)  OK\n";
}

// ─────────────────────────────────────────────────────────
// 4. Cost comparison
//    On x86, relaxed and seq_cst loads are the same instruction.
//    seq_cst stores emit a full memory fence (MFENCE or XCHG),
//    which flushes the store buffer — measurably slower.
//    On ARM the gap is larger: relaxed = no barrier, seq_cst = DMB ISH.
// ─────────────────────────────────────────────────────────
// fetch_add is a read-modify-write and cannot be elided by the compiler.
// This measures the fence overhead added by each memory order.
template<std::memory_order Order>
long long bench_fetch_add(int ops) {
    std::atomic<long long> a{0};
    auto start = Clock::now();
    for (int i = 0; i < ops; ++i)
        a.fetch_add(1, Order);
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - start).count();
}

void cost_comparison() {
    const int OPS = 20'000'000;
    auto relaxed_ns = bench_fetch_add<std::memory_order_relaxed>(OPS);
    auto acqrel_ns  = bench_fetch_add<std::memory_order_acq_rel>(OPS);
    auto seqcst_ns  = bench_fetch_add<std::memory_order_seq_cst>(OPS);

    std::cout << "\n[fetch_add cost, " << OPS / 1'000'000 << "M ops, single thread]\n";
    auto print = [&](const char* name, long long ns) {
        std::cout << "  " << std::left << std::setw(10) << name
                  << std::right << std::setw(5)
                  << static_cast<double>(ns) / OPS << " ns/op\n";
    };
    print("relaxed",  relaxed_ns);
    print("acq_rel",  acqrel_ns);
    print("seq_cst",  seqcst_ns);
}

int main() {
    example_relaxed();
    example_release_acquire();
    example_seq_cst();
    cost_comparison();
    return 0;
}
