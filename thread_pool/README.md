# Thread Pool

A fixed-size worker thread pool in modern C++.

---

## What it does

Creates N threads once at startup and reuses them for every submitted task.
No per-task thread creation. Workers sleep when idle; a task submission wakes exactly one.

```
enqueue(task)
     │
     ▼
 ┌─────────┐     ┌──────────┐
 │  queue  │────►│ worker 0 │──── task()
 │  (mtx)  │     │ worker 1 │──── task()
 │         │     │ worker 2 │──── task()
 └─────────┘     │ worker 3 │──── task()
                 └──────────┘
```

---

## How it works

1. Constructor spawns N threads. Each enters an infinite loop, sleeping on a condition variable.
2. `enqueue()` pushes a `std::function<void()>` onto the queue under a lock, then calls `cv.notify_one()`.
3. One sleeping worker wakes, locks the mutex, takes the front task, **unlocks**, then executes.
4. Destructor sets `stop = true`, calls `cv.notify_all()`, then joins every worker.

---

## Key design decisions

**Unlock before running the task**

```cpp
task = std::move(queue_.front());
queue_.pop();
lock.unlock();   // ← critical
task();          // other workers can pick up tasks simultaneously
```

Holding the mutex during execution serialises all workers. One task runs at a time regardless of thread count — defeating the purpose of a pool.

**`notify_all` in the destructor, not `notify_one`**

```cpp
cv_.notify_all();   // wakes every sleeping worker
```

`notify_one` leaves the remaining workers sleeping permanently. They block forever in `cv_.wait()` and `join()` never returns.

**`unique_lock` for workers, `lock_guard` for `enqueue`**

`cv_.wait()` must release the mutex while sleeping and re-acquire it on wake. Only `unique_lock` supports that. `enqueue` just needs hold-then-release — `lock_guard` is sufficient.

---

## Usage

```cpp
#include "thread_pool.h"

ThreadPool pool(4);

for (int i = 0; i < 8; ++i) {
    pool.enqueue([i] {
        do_work(i);
    });
}
// Destructor blocks until all 8 tasks finish.
```

---

## Build

```bash
cmake -B build && cmake --build build
./build/thread_pool_demo
```

Or directly:

```bash
g++ -std=c++17 -O2 -pthread demo.cpp -o demo && ./demo
```

---

## Concepts

- `std::thread`, `std::mutex`, `std::condition_variable`
- Spurious wakeup protection via predicate lambda
- `unique_lock` vs `lock_guard`
- RAII shutdown with join-on-destroy
