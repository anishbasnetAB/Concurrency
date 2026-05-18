# Concurrency in Modern C++

Exploring concurrency primitives and patterns in modern C++ — thread synchronisation, lock-free data structures, and parallel system design.

## Topics Covered

### SPSC Queue (Single-Producer Single-Consumer)
- Lock-free, wait-free ring buffer
- Cache-line aligned to avoid false sharing
- Used in high-frequency trading and real-time systems

### Thread Pool
- Fixed-size pool of worker threads
- Task queue with mutex + condition variable
- Reusable across concurrent workloads

## Concepts Demonstrated

- `std::thread`, `std::mutex`, `std::condition_variable`
- Lock-free programming with `std::atomic`
- Memory ordering (`memory_order_acquire`, `memory_order_release`)
- False sharing and cache-line optimisation
- Producer-consumer pattern

## How to Build

```bash
g++ -std=c++17 -pthread filename.cpp -o output
./output
```

## Why This Matters

Concurrency is one of the hardest and most sought-after skills in systems programming. These implementations go beyond textbook examples to show a real understanding of hardware and performance.

## Author

[Anish Basnet](https://github.com/anishbasnetAB)
