# SPSC Queue

A lock-free single-producer single-consumer ring buffer in modern C++.

---

## What it is

A fixed-size circular buffer that lets two threads communicate without
any locks, mutexes, or OS involvement. One thread writes. One thread reads.
No blocking. No sleeping. No context switches.

---

## Why

A mutex queue costs ~200–2000 ns per operation — the OS must suspend your
thread, save CPU state, schedule another, and restore. In high-frequency
trading an entire decision happens in 100–500 ns. A single mutex call burns
that budget before any work gets done.

This queue eliminates OS involvement entirely. Two threads communicate through
three atomic variables and nothing else.

**Benchmark (Apple M-series, 10M ops, 2 threads):**

```
SPSC Queue (lock-free):   ~8 ns/op     ~125 Mops/s
Mutex Queue:            ~500 ns/op     ~2   Mops/s
```

---

## Design decisions

### `alignas(64)` — cache line isolation

```cpp
alignas(64) T buf_[N];
alignas(64) std::atomic<size_t> head_{0};
alignas(64) std::atomic<size_t> tail_{0};
```

A CPU fetches memory in 64-byte cache lines. Without alignment, `head_` and
`tail_` share one line. Every time the producer updates `tail_`, it invalidates
the consumer's cache line — forcing a RAM fetch even though `head_` didn't change.
This "false sharing" causes a silent 3–5x slowdown.

`alignas(64)` forces each onto a dedicated line. They never invalidate each other.

### `release` / `acquire` — safe handoff without locks

```
Producer                         Consumer

write buf_[tail]                 read buf_[head]
store tail_ (release) ─────────► load tail_ (acquire)
                      ◄───────── store head_ (release)
                                 load head_ (relaxed)
```

Modern CPUs reorder instructions silently for performance. Without ordering
constraints, the consumer can read stale data after the producer wrote fresh values.

- **release** on the producer's `tail_.store`: everything written above it stays above.
- **acquire** on the consumer's `tail_.load`: everything read below it stays below.

Together they form a happens-before guarantee. No mutex needed.

### `relaxed` — own-variable reads

```cpp
const size_t tail = tail_.load(std::memory_order_relaxed);  // producer reads its own variable
const size_t head = head_.load(std::memory_order_relaxed);  // consumer reads its own variable
```

Each thread exclusively owns one pointer. The producer owns `tail_`; the
consumer owns `head_`. No other thread writes their variable — no ordering hazard.
Relaxed provides atomicity only, at the lowest possible cost.

### Power-of-2 size and bitwise wrapping

```cpp
static_assert((N & (N - 1)) == 0);
static constexpr size_t MASK = N - 1;

const size_t next = (tail + 1) & MASK;   // branchless modulo
```

A compile-time power-of-2 replaces `% N` with a single bitwise AND — one cycle
instead of an integer divide (~20–30 cycles on x86).

---

## How it works

```
head_          tail_
  │              │
  ▼              ▼
[ 3 | 4 | 5 | _ | _ | _ | _ | 0 | 1 | 2 ]
  ^                                       ^
consumer reads                    producer writes

Full  when (tail + 1) & MASK == head
Empty when  head == tail
```

Producer only writes `tail_`. Consumer only writes `head_`.
They never write the same variable — no race condition is possible.

---

## Usage

```cpp
#include "spsc_queue.h"

SPSCQueue<int, 1024> q;   // N must be a power of 2

// producer thread
while (!q.push(value));   // spin until space

// consumer thread
int val;
while (!q.pop(val));      // spin until data
```

---

## Build

```bash
cmake -B build && cmake --build build
./build/spsc_demo
```

Or directly:

```bash
g++ -std=c++17 -O2 -pthread demo.cpp -o spsc_demo && ./spsc_demo
```

---

## Concepts

- Lock-free data structure design
- `std::atomic` with explicit memory ordering
- False sharing and `alignas(64)` cache line isolation
- Bitwise modulo for power-of-2 ring buffers
- Template metaprogramming for zero-cost abstractions
- Nanosecond-precision benchmarking with `std::chrono`
