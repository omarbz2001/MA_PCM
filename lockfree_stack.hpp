#ifndef LOCKFREE_STACK_HPP
#define LOCKFREE_STACK_HPP

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include "task.hpp"

struct LFNode {
    Task* task;
    LFNode* next;
    explicit LFNode(Task* t) : task(t), next(nullptr) {}
};

class LockFreeStack : public TaskCollection {
private:
    // Pack pointer+tag in one 64-bit word: [ tag:16 | ptr:48 ]
    std::atomic<uint64_t> headPacked{0};
    std::atomic<int> size_counter{0};

    static uint64_t pack(LFNode* ptr, uint16_t tag) {
        uint64_t p = reinterpret_cast<uint64_t>(ptr);
        // mask to 48 bits
        p &= 0x0000FFFFFFFFFFFFULL;
        return (static_cast<uint64_t>(tag) << 48) | p;
    }

    static LFNode* unpackPtr(uint64_t packed) {
        uint64_t p = packed & 0x0000FFFFFFFFFFFFULL;
        return reinterpret_cast<LFNode*>(p);
    }

    static uint16_t unpackTag(uint64_t packed) {
        return static_cast<uint16_t>(packed >> 48);
    }

public:
    LockFreeStack() = default;

    ~LockFreeStack() override {
        clear();
    }

    int size() const override { return size_counter.load(std::memory_order_relaxed); }

    Task* operator[](int) override {
        throw std::runtime_error("Index operator not supported for LockFreeStack");
    }

    void push(Task* task) override {
        if (!task) return;
        LFNode* node = new LFNode(task);

        while (true) {
            uint64_t oldPacked = headPacked.load(std::memory_order_acquire);
            LFNode* oldHead = unpackPtr(oldPacked);
            uint16_t oldTag = unpackTag(oldPacked);

            node->next = oldHead;
            uint64_t newPacked = pack(node, static_cast<uint16_t>(oldTag + 1));

            if (headPacked.compare_exchange_weak(oldPacked, newPacked,
                    std::memory_order_release, std::memory_order_acquire)) {
                size_counter.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
    }

    Task* pop() override {
        while (true) {
            uint64_t oldPacked = headPacked.load(std::memory_order_acquire);
            LFNode* oldHead = unpackPtr(oldPacked);
            uint16_t oldTag = unpackTag(oldPacked);

            if (!oldHead) return nullptr;

            LFNode* next = oldHead->next;
            uint64_t newPacked = pack(next, static_cast<uint16_t>(oldTag + 1));

            if (headPacked.compare_exchange_weak(oldPacked, newPacked,
                    std::memory_order_release, std::memory_order_acquire)) {
                Task* t = oldHead->task;
                delete oldHead; // free node structure; task ownership returns to caller
                size_counter.fetch_sub(1, std::memory_order_relaxed);
                return t;
            }
        }
    }

    void clear() override {
        while (true) {
            uint64_t oldPacked = headPacked.load(std::memory_order_acquire);
            LFNode* oldHead = unpackPtr(oldPacked);
            if (!oldHead) break;
            // try to set head to null with tag+1
            uint16_t oldTag = unpackTag(oldPacked);
            uint64_t newPacked = pack(nullptr, static_cast<uint16_t>(oldTag + 1));
            if (headPacked.compare_exchange_weak(oldPacked, newPacked,
                    std::memory_order_release, std::memory_order_acquire)) {
                // drain list
                LFNode* cur = oldHead;
                while (cur) {
                    LFNode* nxt = cur->next;
                    delete cur->task;
                    delete cur;
                    cur = nxt;
                }
                size_counter.store(0, std::memory_order_relaxed);
                break;
            }
        }
    }

    bool empty() const {
        return unpackPtr(headPacked.load(std::memory_order_acquire)) == nullptr;
    }
};

#endif // LOCKFREE_STACK_HPP
