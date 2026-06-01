# Lock-Free Stack

A lock-free stack built on compare-and-swap (CAS), with documentation of the
broader lock-free design pattern.

---

## What "lock-free" means

A data structure is **lock-free** if at least one thread always makes progress,
regardless of what other threads are doing. This contrasts with mutex-based
structures, where a thread can be suspended while holding a lock — blocking
everyone else indefinitely.

| Property      | Mutex-based     | Lock-free          |
|---------------|----------------|--------------------|
| Blocking      | Yes (OS sleep)  | No (spin on CAS)   |
| Starvation    | Possible        | Possible (not wait-free) |
| Overhead      | ~200–2000 ns    | ~10–50 ns          |
| Complexity    | Low             | High               |

---

## Compare-and-swap (CAS)

The atomic building block of lock-free code:

```cpp
// Conceptually:
bool compare_exchange(T& expected, T desired) {
    if (*this == expected) { *this = desired; return true; }
    expected = *this;
    return false;
}
```

CAS is a single indivisible hardware instruction (CMPXCHG on x86, STLXR on ARM).
Lock-free algorithms replace mutexes with a retry loop: read current state, compute
new state, atomically swing the pointer. If someone else changed it first, reload and retry.

---

## The stack

```cpp
void push(T value) {
    Node* node = new Node(std::move(value));
    node->next = head_.load(relaxed);               // read current top
    while (!head_.compare_exchange_weak(             // try to swing head
        node->next, node,                            // on fail: node->next refreshed
        release, relaxed));                          // success: release fence
}

bool pop(T& value) {
    Node* head = head_.load(acquire);               // see everything push() wrote
    while (head) {
        if (head_.compare_exchange_weak(
            head, head->next,                        // try to remove top
            acquire, relaxed)) {
            value = std::move(head->value);
            delete head;
            return true;
        }
    }
    return false;
}
```

### Why `compare_exchange_weak` and not `strong`?

`weak` may spuriously fail even when `head_ == expected`. On RISC architectures
(ARM, RISC-V) this avoids a second conditional branch in the load-linked/store-conditional
pair. Since we're inside a retry loop already, a spurious failure costs one extra
iteration — cheaper than the extra branch `strong` needs.

### Memory orders on CAS

```
push success path:  release  — data_ write is visible before the new head_ is announced
push failure path:  relaxed  — we only reloaded head_, no data to synchronise
pop  success path:  acquire  — we see everything the push() wrote before its release
pop  failure path:  relaxed  — we only refreshed head_, nothing to synchronise
```

---

## The ABA problem

Consider three threads, stack top = A → B:

```
Thread 1: reads head = A, preempted
Thread 2: pops A, pops B, pushes A (same address, recycled)
Thread 1: resumes, CAS(head, A, A.next) succeeds — but A.next is now garbage
```

The CAS sees `head == A` and considers it unchanged, but the stack has been
modified underneath. This implementation uses `delete` on pop, which prevents
the same address from being reused — safe here. In a pool allocator scenario
you need **hazard pointers** or **epoch-based reclamation** (see: Folly's hazptr).

---

## When to use lock-free vs mutex

| Scenario                          | Recommendation      |
|-----------------------------------|---------------------|
| Low-latency hot path (< 1 µs)     | Lock-free           |
| High contention (many threads)    | Lock-free or RCU    |
| Infrequent access, correctness first | Mutex            |
| Complex multi-step transaction    | Mutex (simpler)     |
| Single producer / single consumer | SPSC Queue (even better) |

---

## Usage

```cpp
#include "lock_free_stack.h"

LockFreeStack<int> stack;

// Multiple threads can push concurrently.
stack.push(42);
stack.push(7);

// Pop from a single consumer thread.
int val;
if (stack.pop(val)) {
    process(val);
}
```

---

## Build

```bash
cmake -B build && cmake --build build
./build/lock_free_demo
```

---

## Concepts

- Compare-and-swap (CAS) and `compare_exchange_weak`
- The ABA problem and memory reclamation strategies
- `memory_order_release` / `acquire` on CAS success/failure paths
- Lock-free vs wait-free vs mutex tradeoffs
