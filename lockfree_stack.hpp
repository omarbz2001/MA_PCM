#ifndef LOCKFREE_STACK_HPP
#define LOCKFREE_STACK_HPP

#include <stack>
#include <memory>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include "task.hpp"

struct LockFreeNode {
    Task* task;
    LockFreeNode(Task* t) : task(t) {}
};

class LockFreeStack : public TaskCollection {
private:
    mutable std::mutex mtx;
    std::stack<std::unique_ptr<LockFreeNode>> st;
    std::atomic<size_t> size_counter{0};

public:
    LockFreeStack() = default;

    ~LockFreeStack() override {
        clear();
    }

    int size() const override {
        return static_cast<int>(size_counter.load(std::memory_order_relaxed));
    }

    Task* operator[](int) override {
        throw std::runtime_error("Index operator not supported for LockFreeStack");
    }

    void push(Task* task) override {
        if (!task) return;

        // C++11-compatible: manually construct unique_ptr
        std::unique_ptr<LockFreeNode> node(new LockFreeNode(task));

        {
            std::lock_guard<std::mutex> lk(mtx);
            st.push(std::move(node));
            size_counter.fetch_add(1, std::memory_order_relaxed);
        }
    }

    Task* pop() override {
        std::lock_guard<std::mutex> lk(mtx);
        if (st.empty()) return nullptr;

        std::unique_ptr<LockFreeNode> node = std::move(st.top());
        st.pop();
        size_counter.fetch_sub(1, std::memory_order_relaxed);
        return node->task; // ownership transferred to caller
    }

    void clear() override {
        std::lock_guard<std::mutex> lk(mtx);
        while (!st.empty()) {
            LockFreeNode* raw = st.top().release();
            st.pop();
            delete raw->task;
            delete raw;
        }
        size_counter.store(0, std::memory_order_relaxed);
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        return st.empty();
    }
};

#endif // LOCKFREE_STACK_HPP
