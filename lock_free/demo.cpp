#include <iostream>
#include <thread>
#include "lock_free_stack.h"

int main() {
    LockFreeStack<int> stack;

    // Four threads push concurrently.
    auto push_range = [&](int start, int end) {
        for (int i = start; i < end; ++i)
            stack.push(i);
    };

    std::thread t1(push_range, 0,    250);
    std::thread t2(push_range, 250,  500);
    std::thread t3(push_range, 500,  750);
    std::thread t4(push_range, 750, 1000);
    t1.join(); t2.join(); t3.join(); t4.join();

    // Single thread drains — safe without hazard pointers.
    int count = 0, val;
    while (stack.pop(val)) ++count;

    std::cout << "pushed: 1000  popped: " << count << "\n";
    return 0;
}
