#include <iostream>
#include "thread_pool.h"

int main() {
    ThreadPool pool(4);

    for (int i = 0; i < 8; ++i) {
        pool.enqueue([i] {
            std::cout << "task " << i
                      << " on thread " << std::this_thread::get_id() << "\n";
        });
    }

    // Destructor joins all workers after the last task completes.
    return 0;
}
