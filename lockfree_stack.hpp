#ifndef LOCKFREE_STACK_HPP
#define LOCKFREE_STACK_HPP

#include <atomic>
#include <memory>
#include "task.hpp"

// Tagged pointer to avoid ABA problem
template<typename T>
struct TaggedPointer {
    T* ptr;
    unsigned int tag;
    
    bool operator==(const TaggedPointer& other) const {
        return ptr == other.ptr && tag == other.tag;
    }
};

// Convert tagged pointer to uintptr_t for atomic operations
template<typename T>
union TaggedUint {
    TaggedPointer<T> tagged;
    uintptr_t uint;
    
    TaggedUint() : uint(0) {}
    TaggedUint(T* p, unsigned int t) { tagged.ptr = p; tagged.tag = t; }
    TaggedUint(uintptr_t u) : uint(u) {}
};

// Lock-free stack node
struct LockFreeNode {
    Task* task;
    LockFreeNode* next;
    
    LockFreeNode(Task* t) : task(t), next(nullptr) {}
};

// Main lock-free stack class
class LockFreeStack : public TaskCollection {
private:
    std::atomic<uintptr_t> head;
    std::atomic<size_t> size_counter;
    
    // Helper to get next tag
    unsigned int next_tag() {
        static std::atomic<unsigned int> global_tag{0};
        return global_tag.fetch_add(1, std::memory_order_relaxed);
    }
    
public:
    LockFreeStack() {
        head.store(0, std::memory_order_relaxed);
        size_counter.store(0, std::memory_order_relaxed);
    }
    
    ~LockFreeStack() override {
        // Free remaining nodes (simplified)
        uintptr_t current = head.load(std::memory_order_relaxed);
        if (current != 0) {
            LockFreeNode* node = TaggedUint<LockFreeNode>(current).tagged.ptr;
            delete node;
        }
    }
    
    // TaskCollection interface
    int size() const override {
        return static_cast<int>(size_counter.load(std::memory_order_relaxed));
    }
    
    Task* operator[](int i) override {
        // Not efficient for lock-free stack - avoid using this
        throw std::runtime_error("Index operator not supported for lock-free stack");
        return nullptr;
    }
    
    void push(Task* task) override {
        LockFreeNode* new_node = new LockFreeNode(task);
        TaggedUint<LockFreeNode> new_head(new_node, next_tag());
        
        uintptr_t current_head;
        do {
            current_head = head.load(std::memory_order_relaxed);
            new_node->next = (current_head == 0) ? nullptr 
                : TaggedUint<LockFreeNode>(current_head).tagged.ptr;
            new_head.tagged.tag = next_tag();
        } while (!head.compare_exchange_weak(current_head, 
                                            new_head.uint,
                                            std::memory_order_release,
                                            std::memory_order_relaxed));
        
        size_counter.fetch_add(1, std::memory_order_relaxed);
    }
    
    Task* pop() override {
        TaggedUint<LockFreeNode> current_head;
        TaggedUint<LockFreeNode> new_head;
        
        do {
            current_head.uint = head.load(std::memory_order_relaxed);
            if (current_head.uint == 0) {
                return nullptr; // Stack is empty
            }
            
            LockFreeNode* next_node = current_head.tagged.ptr->next;
            if (next_node == nullptr) {
                new_head.uint = 0;
            } else {
                new_head.tagged.ptr = next_node;
                new_head.tagged.tag = next_tag();
            }
            
        } while (!head.compare_exchange_weak(current_head.uint,
                                            new_head.uint,
                                            std::memory_order_release,
                                            std::memory_order_relaxed));
        
        Task* task = current_head.tagged.ptr->task;
        delete current_head.tagged.ptr;
        
        size_counter.fetch_sub(1, std::memory_order_relaxed);
        return task;
    }
    
    void clear() override {
        // For lock-free stack, we just reset
        head.store(0, std::memory_order_relaxed);
        size_counter.store(0, std::memory_order_relaxed);
    }
    
    // Additional helper
    bool empty() const {
        return size() == 0;
    }
};

#endif // LOCKFREE_STACK_HPP