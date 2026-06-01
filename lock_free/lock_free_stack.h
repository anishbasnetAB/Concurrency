#pragma once
#include <atomic>
#include <utility>

// Lock-free stack using compare-and-swap (CAS).
//
// Safe for concurrent push from multiple threads.
// Safe for pop when only one thread pops at a time (single consumer).
// For multi-consumer use, replace the delete in pop() with a hazard pointer
// or epoch-based reclamation scheme to avoid the ABA problem.
template<typename T>
class LockFreeStack {
    struct Node {
        T value;
        Node* next;
        explicit Node(T v) : value(std::move(v)), next(nullptr) {}
    };

public:
    void push(T value) {
        Node* node = new Node(std::move(value));
        // CAS loop: read head, point node->next at it, try to swing head to node.
        // On failure, compare_exchange_weak updates node->next to the current head.
        node->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(
            node->next, node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    bool pop(T& value) {
        Node* head = head_.load(std::memory_order_acquire);
        while (head) {
            if (head_.compare_exchange_weak(
                head, head->next,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
                value = std::move(head->value);
                delete head;
                return true;
            }
            // CAS failure: head is refreshed to the current head_ by compare_exchange_weak.
        }
        return false;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == nullptr;
    }

    ~LockFreeStack() {
        T val;
        while (pop(val));
    }

    LockFreeStack() = default;
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;

private:
    std::atomic<Node*> head_{nullptr};
};
