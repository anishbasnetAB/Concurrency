# SPSC Ring Buffer

A lock-free single-producer single-consumer ring buffer implemented in modern C++.
Built from scratch to understand the foundations of low-latency systems programming.

---

## What it is

A fixed-size circular buffer that allows two threads to communicate
without any locks, mutexes, or OS involvement.

One thread writes. One thread reads. No blocking. No sleeping. No context switches.

---

## Why it exists

A mutex-based queue costs ~2,000–10,000ns per operation.
The OS must suspend your thread, save CPU state, schedule another thread,
then restore everything when you wake up.

In high-frequency trading, an entire trade decision happens in 100–500ns.
A single mutex call burns that budget before any work gets done.

This queue eliminates OS involvement entirely.
Two threads communicate through three atomic variables and nothing else.

---

## Benchmark

Hardware: Apple Silicon MacBook Pro
Operations: 10,000,000



8 nanoseconds. One operation in 8 billionths of a second.

---

## Design decisions

### alignas(64) — cache line isolation

``` cpp
alignas(64) std::atomic<size_t> head_{0};
alignas(64) std::atomic<size_t> tail_{0};
alignas(64) T data_[N];


A CPU fetches memory in 64-byte chunks called cache lines.
Without alignment, head_ and tail_ share one cache line.
Every time the producer updates tail_, it invalidates the consumer's cache line —
forcing a RAM fetch even though head_ did not change.

alignas(64) forces each variable onto its own dedicated cache line.
Producer and consumer never invalidate each other. Pure L1 cache speed.

### memory_order_release / acquire — safe handoff without locks

```cpp
// producer — announces new data
tail_.store(next, std::memory_order_release);

// consumer — sees producer's data
tail_.load(std::memory_order_acquire);
```

Modern CPUs reorder instructions silently for performance.
Without ordering constraints, the consumer can read stale data
even after the producer has written fresh values.

release on the producer store: everything written above stays above.
acquire on the consumer load: everything read below stays below.
Together they form a happens-before guarantee across two threads.
No mutex needed.

### memory_order_relaxed — own variable reads

```cpp
size_t tail = tail_.load(std::memory_order_relaxed);  // producer reads own variable
size_t head = head_.load(std::memory_order_relaxed);  // consumer reads own variable
```

Each thread exclusively owns one pointer.
Producer owns tail_. Consumer owns head_.
No other thread writes their variable — no ordering danger.
Relaxed gives atomicity only — the fastest possible memory order.

### Template parameters — compile time size

```cpp
template<typename T, size_t N>
struct SPSCQueue
```

Buffer size N is a template parameter, not a constructor argument.
The compiler knows the exact size at compile time —
no heap allocation, no pointer indirection, maximum cache friendliness.
Works for any type T with zero runtime overhead.

---

## How it works

```
Producer                          Consumer

load tail_ (relaxed)              load head_ (relaxed)
load head_ (acquire)              load tail_ (acquire)
check not full                    check not empty
write data_[tail]                 read data_[head]
store tail_ (release) ─────────► acquire load sees fresh data
                      ◄───────── store head_ (release)
```

Producer only writes tail_. Consumer only writes head_.
They never write the same variable. No race condition possible.

---

## Usage

```cpp
SPSCQueue<int, 1024> queue;

// producer thread
while(!queue.push(value));

// consumer thread
int val;
while(!queue.pop(val));
```

---

## Build

```bash
g++ -std=c++17 -O2 -pthread spsc_queue.cpp -o spsc
./spsc
```

---

## Concepts covered

- Lock-free data structure design
- std::atomic with explicit memory ordering
- Cache line alignment and false sharing elimination
- Template metaprogramming for zero-cost abstractions
- Nanosecond-precision benchmarking with std::chrono
- Producer-consumer synchronisation without OS involvement
```
