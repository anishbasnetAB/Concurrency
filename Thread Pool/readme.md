# Thread Pool

A worker thread pool in modern C++ built from scratch.

## What it does
Fixed number of threads created once at startup and reused for every task.
No thread creation overhead per task. Workers sleep when idle, wake when work arrives.

## How it works
- Workers block on a condition variable when the queue is empty
- `enqueue()` pushes a task and wakes one worker
- Task runs outside the mutex lock — true parallelism between workers
- Destructor sets stop flag, wakes all workers, joins cleanly

## Key design decisions

**Why unlock before running the task**
Holding the mutex during execution serialises all workers.
Releasing it first lets other workers pick up tasks simultaneously.

**Why notify_all in destructor**
`notify_one` wakes a single thread. Remaining workers sleep forever and never join.
`notify_all` wakes every worker so each can see the stop flag and exit cleanly.

**Why lock_guard in enqueue but unique_lock in workers**
`enqueue` only needs automatic unlock — `lock_guard` is sufficient.
Workers need to release the lock mid-life while sleeping — only `unique_lock` can do that.

## Usage
```cpp
ThreadPool pool(4);

for(int i = 0; i < 8; i++) {
    pool.enqueue([i] {
        std::cout << "task " << i << "\n";
    });
}
// destructor joins all threads automatically
```

## Build
```bash
g++ -std=c++17 -pthread thread_pool.cpp -o thread_pool
./thread_pool
```

## Concepts covered
- `std::thread` and `std::mutex`
- `std::condition_variable` and spurious wakeup protection
- `std::unique_lock` vs `std::lock_guard`
- RAII lock management
- Graceful shutdown pattern