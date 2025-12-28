#ifndef PARALLEL_TASK_RUNNER_HPP
#define PARALLEL_TASK_RUNNER_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include "lockfree_stack.hpp"
#include "task.hpp"

class ParallelTaskRunner : public TaskRunner {
private:
    LockFreeStack task_pool;
    std::vector<std::thread> workers;
    std::atomic<bool> termination_requested;
    std::atomic<int> active_workers;
    std::atomic<int> total_tasks;
    
    int _num_threads;
    
    void worker_function(int thread_id) {
        active_workers.fetch_add(1, std::memory_order_relaxed);
        
        while (!termination_requested.load(std::memory_order_relaxed)) {
            Task* task = task_pool.pop();
            
            if (task == nullptr) {
                // If no task and no total tasks left, exit
                if (total_tasks.load(std::memory_order_acquire) == 0) {
                    break;
                }
                // Otherwise, yield and try again
                std::this_thread::yield();
                continue;
            }
            
            // Process the task
            int n = task->split(&task_pool);
            if (n > 0) {
                // New subtasks created, add to total_tasks
                total_tasks.fetch_add(n, std::memory_order_relaxed);
            } else {
                // Leaf task, solve it
                task->solve();
            }
            // Decrement total_tasks for this processed task
            total_tasks.fetch_sub(1, std::memory_order_acq_rel);
            delete task;
        }
        
        active_workers.fetch_sub(1, std::memory_order_relaxed);
    }
    
public:
    ParallelTaskRunner(int num_threads) 
        : _num_threads(num_threads),
          termination_requested(false), 
          active_workers(0),
          total_tasks(0) {
        
        if (_num_threads <= 0) {
            _num_threads = std::thread::hardware_concurrency();
            if (_num_threads == 0) _num_threads = 4;
        }
        
        workers.reserve(_num_threads);
    }
    
    ~ParallelTaskRunner() override {
        stop();
    }
    
    virtual void run(Task* root_task) override {
        if (!root_task) return;
        
        termination_requested.store(false, std::memory_order_relaxed);
        total_tasks.store(1, std::memory_order_relaxed);
        
        task_pool.clear();
        task_pool.push(root_task);
        
        startTimer();
        
        std::cout << "Creating " << _num_threads << " worker threads\n";
        
        for (int i = 0; i < _num_threads; ++i) {
            workers.emplace_back(&ParallelTaskRunner::worker_function, this, i);
        }
        
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        workers.clear();
        
        stopTimer();
        
        std::cout << "All threads finished.\n";
    }
    
    void stop() {
        termination_requested.store(true, std::memory_order_relaxed);
        
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers.clear();
    }
};

#endif