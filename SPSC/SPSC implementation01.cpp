#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

// Fixed-size lock-free queue for exactly one producer and one consumer.
// No mutexes. No OS calls. Just atomics and cache-friendly memory layout.
template<typename T, size_t N>
struct SPSCQueue {

    // Called by the producer thread only.
    // Returns false if the buffer is full — caller should retry.
    bool push(const T& val) {
        // We own tail_ so relaxed is safe — no other thread writes it
        size_t tail = tail_.load(std::memory_order_relaxed);

        // head_ belongs to the consumer — acquire ensures we see
        // the latest value after any pops the consumer has done
        size_t head = head_.load(std::memory_order_acquire);

        // No room — one slot is always kept empty to distinguish full from empty
        if((tail + 1) % N == head) return false;

        // Write the value before we announce it
        data_[tail] = val;

        // Release fence — data_ write cannot reorder past this store.
        // Consumer will see fresh data after its matching acquire load.
        tail_.store((tail + 1) % N, std::memory_order_release);
        return true;
    }

    // Called by the consumer thread only.
    // Returns false if the buffer is empty — caller should retry.
    bool pop(T& val) {
        // tail_ belongs to the producer — acquire ensures we see
        // everything the producer wrote before its release store
        size_t tail = tail_.load(std::memory_order_acquire);

        // We own head_ so relaxed is safe here
        size_t head = head_.load(std::memory_order_relaxed);

        // Nothing to read
        if(tail == head) return false;

        // Read the value before advancing head
        val = data_[head];

        // Release fence — signals the producer that this slot is now free
        head_.store((head + 1) % N, std::memory_order_release);
        return true;
    }

private:
    // Each variable on its own 64-byte cache line.
    // Without this, producer and consumer would fight over the same
    // cache line on every operation — silent 3-5x slowdown.
    alignas(64) T data_[N];
    alignas(64) std::atomic<size_t> head_{0};  // consumer advances this
    alignas(64) std::atomic<size_t> tail_{0};  // producer advances this
};

int main() {
    SPSCQueue<int, 1024> queue;
    const int OPS = 10'000'000;

    // Start timer after queue is created — we are measuring
    // throughput of push/pop, not object construction
    auto start = std::chrono::high_resolution_clock::now();

    // Producer spins on push if the buffer is full
    std::thread producer([&]{
        for(int i = 0; i < OPS; i++)
            while(!queue.push(i));
    });

    // Consumer spins on pop if the buffer is empty.
    // volatile sink prevents the compiler from optimising away the read —
    // without it the compiler sees val is unused and skips the pop entirely
    std::thread consumer([&]{
        int val;
        for(int i = 0; i < OPS; i++){
            while(!queue.pop(val));
            volatile int sink = val;
        }
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>
              (end - start).count();

    std::cout << "total time:    " << ns << " ns\n";
    std::cout << "per operation: " << ns / OPS << " ns\n";

    return 0;
}