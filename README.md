# Concurrency

Four concurrency primitives built from scratch in modern C++17, with
documentation and a benchmark suite comparing each against its mutex-based
equivalent.

---

## Structure

```
Concurrency/
├── thread_pool/        Fixed-size worker pool — mutex + condition variable
├── spsc/               Lock-free ring buffer — atomics, no OS involvement
├── lock_free/          Lock-free stack — compare-and-swap (CAS)
├── memory_ordering/    Atomic memory orders — relaxed, release/acquire, seq_cst
└── benchmarks/         Unified suite comparing all structures
```

---

## Benchmark results (Apple M-series, 14 hardware threads)

| Structure                     | Scenario                          | Latency    | Throughput      |
|------------------------------|-----------------------------------|-----------|-----------------|
| SPSC Queue (lock-free)        | push+pop, 10M ops, 2 threads      | 11.5 ns   | 87 Mops/s       |
| Mutex Queue                   | push+pop, 10M ops, 2 threads      | 82.7 ns   | 12 Mops/s       |
| Lock-Free Stack               | push+pop, 1M ops, single-thread   | 8.1 ns    | 123 Mops/s      |
| Mutex Stack (no contention)   | push+pop, 1M ops, single-thread   | 4.0 ns    | 252 Mops/s      |
| Thread Pool — light tasks     | 400 tasks × 100 µs, 14 workers    | —         | 95K tasks/s     |
| Thread Pool — light tasks     | 400 tasks × 100 µs, 1 worker      | —         | 11K tasks/s     |
| `relaxed` / `seq_cst`         | fetch_add, 20M ops, single-thread | ~1.6 ns   | ~630 Mops/s     |

**Takeaways:**
- SPSC queue is **7× faster** than a mutex queue under producer-consumer load.
- An uncontended mutex is faster than CAS in the single-threaded case — lock-free wins under contention.
- Thread pool scales **~9× across 14 cores** when tasks are large enough (>> scheduling overhead).
- On Apple Silicon, `relaxed` and `seq_cst` fetch_add cost the same; the gap is larger under cross-core contention and on weaker-memory-model CPUs (ARM cortex).

---

## Quick start

```bash
git clone <repo>
cd Concurrency
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run everything
./build/bench             # full benchmark suite
./build/thread_pool_demo
./build/spsc_demo
./build/lock_free_demo
./build/memory_ordering
```

Requires: C++17 compiler, CMake ≥ 3.15, pthreads.

---

## Components

### [Thread Pool](thread_pool/)

Fixed number of threads created once. Workers block on a condition variable;
`enqueue()` wakes one. Unlocks the mutex before running each task so workers
execute in parallel. Clean shutdown via RAII destructor.

**Key insight:** Holding the mutex during task execution serialises all workers
into a single thread — defeating the purpose of a pool.

### [SPSC Queue](spsc/)

Lock-free ring buffer for exactly one producer and one consumer. Uses
`memory_order_release/acquire` to synchronise data without a mutex, and
`alignas(64)` to keep producer and consumer on separate cache lines.

**Key insight:** False sharing between `head_` and `tail_` causes a 3–5x
slowdown even when the cache lines are never written simultaneously — `alignas`
eliminates this silently.

### [Lock-Free Stack](lock_free/)

CAS-based stack. `push` retries until it atomically swings `head_` to a new
node; `pop` retries until it atomically removes the top. Documents the ABA
problem and when to use hazard pointers.

**Key insight:** `compare_exchange_weak` may spuriously fail — use it inside
retry loops; `strong` adds a branch that costs more than an extra loop iteration.

### [Memory Ordering](memory_ordering/)

Runnable examples for `relaxed`, `release/acquire`, and `seq_cst`, each with a
correctness assertion and a timing measurement. Includes the Dekker litmus test
showing when `seq_cst` is required over `release/acquire`.

**Key insight:** `release/acquire` synchronises a pair of variables;
`seq_cst` gives a single global order across all atomic variables — stronger,
but costs a memory fence on every store.

---

## Design philosophy

Each primitive is header-only and self-contained. No external dependencies.
Documentation explains *why* each decision was made, not just what the code does.
