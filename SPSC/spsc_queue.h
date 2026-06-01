#pragma once
#include <atomic>

// Fixed-size, lock-free, single-producer single-consumer ring buffer.
// N must be a power of 2; the bitwise mask replaces the % operator.
template<typename T, size_t N>
class SPSCQueue {
    static_assert(N >= 2 && (N & (N - 1)) == 0, "N must be a power of 2");
    static constexpr size_t MASK = N - 1;

public:
    // Called by the producer thread only.
    bool push(const T& val) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next = (tail + 1) & MASK;
        if (next == head_.load(std::memory_order_acquire)) return false;  // full
        buf_[tail] = val;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    // Called by the consumer thread only.
    bool pop(T& val) {
        const size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) return false;  // empty
        val = buf_[head];
        head_.store((head + 1) & MASK, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

private:
    // Each on its own cache line — prevents false sharing between producer and consumer.
    alignas(64) T buf_[N];
    alignas(64) std::atomic<size_t> head_{0};  // consumer advances
    alignas(64) std::atomic<size_t> tail_{0};  // producer advances
};
