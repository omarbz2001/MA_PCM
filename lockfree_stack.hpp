#ifndef LOCKFREE_STACK_HPP
#define LOCKFREE_STACK_HPP

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <cassert>
#include <new>

#include "task.hpp"

// Treiber lock-free stack implementation (CAS-based).
// Class name is LockFreeStack so it can be used as a drop-in replacement.
//
// Implementation notes:
// - This packs (pointer, 16-bit counter) into a single uint64_t value to
//   mitigate simple ABA races. The pointer bits are shifted left by 16
//   and the lower 16 bits store a small counter.
// - This requires 64-bit pointers (asserted). For absolute safety in very
//   heavy concurrent workloads, a real safe reclamation scheme (hazard
//   pointers or epoch-based reclamation) is recommended.
// - The API matches TaskCollection used by the runner.

struct LockFreeNode {
    Task* task;
    // next pointer packed similarly: (ptr << 16) | counter
    std::atomic<uint64_t> next;
    LockFreeNode(Task* t) : task(t), next(0) {}
    LockFreeNode() : task(nullptr), next(0) {}
};

class LockFreeStack : public TaskCollection {
private:
    // packed pointer format: [ pointer (upper 48 bits) | counter (lower 16 bits) ]
    static_assert(sizeof(void*) == 8, "LockFreeStack requires 64-bit pointers");

    std::atomic<uint64_t> head; // packed pointer+counter
    std::atomic<int> size_counter{0};

    static inline uint64_t pack(LockFreeNode* p, uint16_t counter) {
        uint64_t ptr_part = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
        return (ptr_part << 16) | static_cast<uint64_t>(counter);
    }
    static inline LockFreeNode* unpack_ptr(uint64_t packed) {
        return reinterpret_cast<LockFreeNode*>(static_cast<uintptr_t>(packed >> 16));
    }
    static inline uint16_t unpack_cnt(uint64_t packed) {
        return static_cast<uint16_t>(packed & 0xFFFFu);
    }

public:
    LockFreeStack() {
        // empty stack: head = pack(nullptr, 0)
        head.store(pack(nullptr, 0), std::memory_order_relaxed);
    }

    ~LockFreeStack() override {
        clear();
    }

    int size() const override {
        return static_cast<int>(size_counter.load(std::memory_order_acquire));
    }

    Task* operator[](int) override {
        throw std::runtime_error("Index operator not supported for LockFreeStack");
    }

    void push(Task* task) override {
        if (!task) return;

        // allocate node
        LockFreeNode* node = static_cast<LockFreeNode*>(std::malloc(sizeof(LockFreeNode)));
        if (!node) throw std::bad_alloc();
        // placement-new initialize
        new (node) LockFreeNode(task);
        node->next.store(pack(nullptr, 0), std::memory_order_relaxed);

        while (true) {
            uint64_t old_head = head.load(std::memory_order_acquire);
            LockFreeNode* old_ptr = unpack_ptr(old_head);
            uint16_t old_cnt = unpack_cnt(old_head);

            // set node->next to old_head (pointer part)
            uint64_t new_next_packed = pack(old_ptr, old_cnt);
            node->next.store(new_next_packed, std::memory_order_relaxed);

            // attempt to swing head to new node with incremented counter
            uint64_t new_head = pack(node, static_cast<uint16_t>(old_cnt + 1));
            if (head.compare_exchange_weak(old_head, new_head,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
                size_counter.fetch_add(1, std::memory_order_release);
                return;
            }
            // CAS failed: old_head updated with latest value by compare_exchange_weak
            // retry (and update node->next accordingly on next loop)
        }
    }

    Task* pop() override {
        while (true) {
            uint64_t old_head = head.load(std::memory_order_acquire);
            LockFreeNode* old_ptr = unpack_ptr(old_head);
            uint16_t old_cnt = unpack_cnt(old_head);

            if (old_ptr == nullptr) {
                // empty stack
                return nullptr;
            }

            // read next from old_ptr
            uint64_t next_packed = old_ptr->next.load(std::memory_order_acquire);
            LockFreeNode* next_ptr = unpack_ptr(next_packed);
            uint16_t next_cnt = unpack_cnt(next_packed);

            // attempt to swing head to next_ptr with incremented counter
            uint64_t new_head = pack(next_ptr, static_cast<uint16_t>(old_cnt + 1));
            if (head.compare_exchange_weak(old_head, new_head,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
                // Successfully popped old_ptr
                Task* t = old_ptr->task;

                // destroy node and free memory
                old_ptr->~LockFreeNode();
                std::free(old_ptr);

                size_counter.fetch_sub(1, std::memory_order_release);
                return t;
            }
            // else CAS failed: retry
        }
    }

    void clear() override {
        // Pop everything and delete tasks
        Task* t;
        while ((t = pop()) != nullptr) {
            delete t;
        }
        // ensure counters are zeroed
        size_counter.store(0, std::memory_order_relaxed);
        head.store(pack(nullptr, 0), std::memory_order_relaxed);
    }

    bool empty() const {
        uint64_t h = head.load(std::memory_order_acquire);
        return (unpack_ptr(h) == nullptr);
    }
};

#endif // LOCKFREE_STACK_HPP
