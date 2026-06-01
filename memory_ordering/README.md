# Memory Ordering

How CPUs and compilers reorder memory operations, why it matters for concurrent
code, and how C++ `std::atomic` memory orders let you express exactly the
guarantees you need — no more, no less.

---

## The problem

Modern CPUs execute instructions out of order for performance. A store to memory
may not be visible to other cores in the order it was issued. Without explicit
constraints, this is valid:

```
Thread 1 (writer)         Thread 2 (reader)
─────────────────         ─────────────────
data = 42;                if (ready) {
ready = true;                 // data might still be 0!
                          }
```

The CPU is allowed to reorder the two stores in Thread 1. Thread 2 can observe
`ready == true` before `data == 42`.

---

## C++ memory orders

C++ exposes six memory orders. Three matter in practice:

### `relaxed` — atomicity only

```cpp
counter.fetch_add(1, std::memory_order_relaxed);
```

- The operation is atomic (no torn reads/writes).
- No ordering guarantee relative to any other operation.
- Use when threads share a counter but don't use it to signal other data.
- Fastest — no memory fence emitted on x86 or ARM.

### `release` / `acquire` — happens-before across threads

```cpp
// Thread 1 — producer
data = 42;
flag.store(true, std::memory_order_release);   // "data is ready"

// Thread 2 — consumer
while (!flag.load(std::memory_order_acquire)); // wait
assert(data == 42);                            // always safe
```

- `release` store: all writes above it are committed before the store is visible.
- `acquire` load: all reads below it happen after the load.
- Together they form a **happens-before** edge: everything before the release is
  visible after the acquire.
- This is the right tool for producer-consumer patterns — cheaper than seq_cst.

Use `acq_rel` (both at once) for read-modify-write operations like `fetch_add` on
a shared variable that also synchronises other data.

### `seq_cst` — single total order (the default)

```cpp
x.store(true);              // seq_cst by default
y.store(true);              // seq_cst by default

// Another thread:
while (!x.load()); if (y.load()) ++z;   // z is never 0
```

- Guarantees a **single global order** across ALL seq_cst operations on ALL atomic
  variables across ALL threads.
- Release/acquire only orders pairs of variables; seq_cst orders everything.
- On x86, seq_cst stores emit `MFENCE` or `XCHG` — flushing the store buffer.
- On ARM, stores emit `DMB ISH` — a full data memory barrier.
- Use when you need multiple threads to agree on the relative order of events
  on different variables (the Dekker/store-buffer litmus test).

---

## Cost comparison (Apple M-series, 20M stores, single thread)

```
relaxed:    0 ns/op   (no fence)
release:    0 ns/op   (acquire/release is free on TSO architectures like x86)
seq_cst:   ~3 ns/op   (store buffer flush — more noticeable under contention)
```

On ARM the gap is larger: `relaxed` is a plain store, `seq_cst` emits a barrier.

---

## Decision guide

```
Do you need ordering?
├── No  → relaxed  (counters, per-thread state)
└── Yes → Does it span multiple atomic variables?
          ├── No  → release/acquire  (flag + payload pairs)
          └── Yes → seq_cst          (Dekker-style mutual exclusion, consensus)
```

---

## The `consume` order (informational)

`memory_order_consume` exists for dependency chains (pointer chasing). It was
weakened in C++17 to "temporarily discouraged" because compilers universally
promoted it to `acquire`. Treat it as `acquire` in practice.

---

## Build and run examples

```bash
cmake -B build && cmake --build build
./build/memory_ordering
```

Each example prints its result and an `OK` assertion.
The final section shows the measured cost of each order on your hardware.

---

## Concepts

- CPU reordering and store buffers (TSO vs relaxed memory models)
- Happens-before and synchronises-with relationships
- `relaxed`, `release/acquire`, `acq_rel`, `seq_cst`
- The Dekker/store-buffer litmus test
- Hardware fence instructions (MFENCE, DMB ISH)
