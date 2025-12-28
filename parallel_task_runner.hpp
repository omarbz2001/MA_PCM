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
    std::atomic<int> total_tasks;
    std::atomic<int> nbr_of_tasks_done;
    
    int _num_threads;
    
    void worker_function(int thread_id) {

        while (!termination_requested.load(std::memory_order_relaxed)) {
            TSPTask* task = static_cast<TSPTask*>(task_pool.pop());
            
            if (task == nullptr) {
                // If no task and no total tasks left, exit
                if (total_tasks.load(std::memory_order_acquire) == 0) {
                    break;
                }

                // TODO EXPENENTIAL BACKOFF
                std::this_thread::sleep_for(std::chrono::microseconds(1 << std::min(thread_id, 6)));

                // Otherwise, yield and try again
                std::this_thread::yield();
                continue;
            }
            
            // Process the task
            int nbrOfChildTasksPruned = 0;
            int n = task->split(&task_pool, nbrOfChildTasksPruned);
            if (n > 0) {
                // New subtasks created
            } else {
                // Leaf task, solve it
                task->solve();
            }
            total_tasks.fetch_sub(nbrOfChildTasksPruned + 1, std::memory_order_acq_rel);
            nbr_of_tasks_done.fetch_add(1, std::memory_order_acq_rel);
            delete task;
        }
    }
    
public:
    ParallelTaskRunner(int num_threads) 
        : _num_threads(num_threads),
          total_tasks(0),
          termination_requested(false),
          nbr_of_tasks_done(0){
        
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
        throw;
    }
    
    virtual void run(TSPTask* root_task) {
        if (!root_task) return;
        
        termination_requested.store(false, std::memory_order_relaxed);
        task_pool.clear();
        task_pool.push(root_task);
        
        startTimer();

        total_tasks.store(root_task->remainingLeaves(true), std::memory_order_relaxed);

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