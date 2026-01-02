#ifndef WORK_STEALING_DEQUE_HPP
#define WORK_STEALING_DEQUE_HPP

#include <atomic>
#include <vector>
#include <memory>
#include "task.hpp"

/*
 * Chase–Lev work-stealing deque
 */
class WorkStealingDeque : public TaskCollection {
private:
    std::vector<Task*> buffer;
    std::atomic<int> top;
    std::atomic<int> bottom;
    const int capacity;

public:
    explicit WorkStealingDeque(int cap = 1 << 15)
        : buffer(cap),
          top(0),
          bottom(0),
          capacity(cap) {}

    ~WorkStealingDeque() override = default;

    int size() const override {
        int b = bottom.load(std::memory_order_relaxed);
        int t = top.load(std::memory_order_relaxed);
        return b - t;
    }

    Task* operator[](int) override {
        throw std::runtime_error("Indexing not supported on WorkStealingDeque");
    }

    void push(Task* t) override {
        int b = bottom.load(std::memory_order_relaxed);
        buffer[b % capacity] = t;
        std::atomic_thread_fence(std::memory_order_release);
        bottom.store(b + 1, std::memory_order_relaxed);
    }

    Task* pop() override {
        int b = bottom.load(std::memory_order_relaxed) - 1;
        bottom.store(b, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);

        int t = top.load(std::memory_order_relaxed);
        if (t <= b) {
            Task* task = buffer[b % capacity];
            if (t == b) {
                if (!top.compare_exchange_strong(
                        t, t + 1,
                        std::memory_order_seq_cst,
                        std::memory_order_relaxed)) {
                    task = nullptr;
                }
                bottom.store(b + 1, std::memory_order_relaxed);
            }
            return task;
        } else {
            bottom.store(b + 1, std::memory_order_relaxed);
            return nullptr;
        }
    }

    Task* steal() {
        int t = top.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        int b = bottom.load(std::memory_order_acquire);

        if (t < b) {
            Task* task = buffer[t % capacity];
            if (top.compare_exchange_strong(
                    t, t + 1,
                    std::memory_order_seq_cst,
                    std::memory_order_relaxed)) {
                return task;
            }
        }
        return nullptr;
    }

    void clear() override {
        top.store(0, std::memory_order_relaxed);
        bottom.store(0, std::memory_order_relaxed);
    }
};

#endif // WORK_STEALING_DEQUE_HPP
