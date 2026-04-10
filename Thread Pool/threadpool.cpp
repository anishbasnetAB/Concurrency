#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>

// A pool of worker threads that reuse themselves
// instead of creating a new thread for every task
class ThreadPool {
public:

    // Spin up n workers at startup — they live until destructor
    ThreadPool(int n) {
        for(int i = 0; i < n; i++) {
            t.emplace_back([this] {
                
                // Each worker loops forever waiting for tasks
                while(true) {
                    
                    // Lock before touching the shared queue
                    std::unique_lock<std::mutex> lock(mtx);
                    
                    // Sleep until a task arrives or we are shutting down
                    // Lambda guards against spurious wakeups
                    cv.wait(lock, [this] {
                        return stop || !task.empty();
                    });
                    
                    // Shutdown signal — but finish remaining tasks first
                    if(stop && task.empty()) {
                        return;
                    }
                    
                    // Grab the next task and remove it from the queue
                    std::function<void()> fn = task.front();
                    task.pop();
                    
                    // Unlock BEFORE running the task
                    // If we held the lock during execution,
                    // all workers would be serialised — no real parallelism
                    lock.unlock();
                    
                    // Run the task — other workers can pick up tasks simultaneously
                    fn();
                }
            });
        }
    }

    // Public API — anyone can submit a task from outside
    void enqueue(std::function<void()> fn) {
        {
            // Lock just long enough to push onto the queue
            std::lock_guard<std::mutex> lock(mtx);
            task.push(fn);
        }
        // Notify outside the lock — worker wakes and gets mutex immediately
        cv.notify_one();
    }

    // Clean shutdown — finish all tasks, then stop all workers
    ~ThreadPool() {
        {
            // Signal all workers to stop after finishing remaining tasks
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        // Wake ALL workers — not just one
        // notify_one would leave remaining workers sleeping forever
        cv.notify_all();
        
        // Wait for every worker to finish cleanly
        for(auto& thread : t)
            thread.join();
    }

private:
    std::vector<std::thread> t;              // the worker threads
    std::condition_variable cv;              // sleep and wake mechanism
    std::mutex mtx;                          // protects the task queue
    std::queue<std::function<void()>> task;  // pending tasks waiting to run
    bool stop = false;                       // shutdown flag
};

int main() {
    // Create pool with 4 workers
    ThreadPool pool(4);

    // Submit 8 tasks — workers pick them up as they become free
    for(int i = 0; i < 8; i++) {
        pool.enqueue([i] {
            std::cout << "task " << i 
                      << " running on thread " 
                      << std::this_thread::get_id() << "\n";
        });
    }

    // Destructor runs here automatically
    // Waits for all 8 tasks to complete before exiting
    return 0;
}