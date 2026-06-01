#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t n) {
        workers_.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mu_);
                        cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                        if (stop_ && queue_.empty()) return;
                        task = std::move(queue_.front());
                        queue_.pop();
                    }
                    // Unlock before running — holds the mutex during execution
                    // would serialise all workers into a single thread.
                    task();
                }
            });
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard lock(mu_);
            queue_.push(std::move(task));
        }
        cv_.notify_one();
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mu_);
            stop_ = true;
        }
        // notify_all — otherwise workers that never received a task sleep forever.
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> queue_;
    std::mutex mu_;
    std::condition_variable cv_;
    bool stop_ = false;
};
