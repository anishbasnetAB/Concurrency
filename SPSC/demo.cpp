#include <iostream>
#include <thread>
#include <chrono>
#include "spsc_queue.h"

int main() {
    SPSCQueue<int, 1024> q;
    const int OPS = 10'000'000;

    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&] {
        for (int i = 0; i < OPS; ++i)
            while (!q.push(i));
    });

    std::thread consumer([&] {
        int val;
        for (int i = 0; i < OPS; ++i) {
            while (!q.pop(val));
            (void)val;
        }
    });

    producer.join();
    consumer.join();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    std::cout << "ops:           " << OPS << "\n";
    std::cout << "total time:    " << ns / 1'000'000 << " ms\n";
    std::cout << "per operation: " << ns / OPS << " ns\n";
    return 0;
}
