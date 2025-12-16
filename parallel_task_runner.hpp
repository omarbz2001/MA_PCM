#ifndef PARALLEL_TASK_RUNNER_HPP
#define PARALLEL_TASK_RUNNER_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <condition_variable>
#include <mutex>

#include "task.hpp"
#include "lockfree_stack.hpp"

// ParallelTaskRunner:
// - Uses a global lock-free Treiber stack for tasks
// - Uses a condition variable ONLY for sleeping/wakeup
// - Termination is driven by outstanding_tasks

class ParallelTaskRunner : public TaskRunner {
private:
    LockFreeStack task_pool; // lock-free Treiber stack
    std::vector<std::thread> workers;
    std::atomic<bool> termination_requested;
    std::atomic<int> active_workers;
    std::atomic<int> tasks_processed;
    std::atomic<int> tasks_created;
    std::atomic<int> idle_threads;
    std::atomic<int> outstanding_tasks;
    std::atomic<int> total_idle_loops;
    std::atomic<int> total_work_loops;
    
    
    int _num_threads;

    void worker_function(int /*thread_id*/) {
        active_workers.fetch_add(1, std::memory_order_relaxed);
        
        int idle_loops = 0;
        const int MAX_IDLE_LOOPS = 1000;
        
        while (!termination_requested.load(std::memory_order_relaxed)) {
            Task* task = task_pool.pop();
            
            if (task == nullptr) {
                total_idle_loops.fetch_add(1, std::memory_order_relaxed);
                idle_loops++;
                idle_threads.fetch_add(1, std::memory_order_relaxed);
                
                
                // termination: no tasks outstanding and pool empty
                if (outstanding_tasks.load(std::memory_order_acquire) == 0 && task_pool.empty()) {
                    
                    idle_threads.fetch_sub(1, std::memory_order_relaxed);
                    break;
                }
                
               
                // light backoff to reduce contention without burning CPU
                std::this_thread::sleep_for(std::chrono::microseconds(4));
                idle_threads.fetch_sub(1, std::memory_order_relaxed);
                
                // keep yielding rather than hard-exiting; termination
                // is driven by outstanding_tasks reaching zero
                if (idle_loops > MAX_IDLE_LOOPS) idle_loops = 0;
                
                std::this_thread::yield();
                continue;
            }
            
            idle_loops = 0;  
            
           
            int n = task->split(&task_pool);
            total_work_loops.fetch_add(1, std::memory_order_relaxed);
            if (n > 0) {
                tasks_created.fetch_add(n, std::memory_order_relaxed);
                // new children become outstanding work
                outstanding_tasks.fetch_add(n, std::memory_order_relaxed);
                delete task;
            } else {
                task->solve();
                delete task;
                tasks_processed.fetch_add(1, std::memory_order_relaxed);
            }

            // one logical task (this one) is completed
            int remaining = outstanding_tasks.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (remaining == 0) {
                // encourage other threads to exit quickly on next idle check
                idle_loops = MAX_IDLE_LOOPS + 1;
            }
        }

        active_workers.fetch_sub(1, std::memory_order_relaxed);
    }

public:
    ParallelTaskRunner(int num_threads) 
        : _num_threads(num_threads),
          termination_requested(false), 
          active_workers(0),
          tasks_processed(0),
          tasks_created(0),
          idle_threads(0),
                    outstanding_tasks(0),
                    total_idle_loops(0),
                    total_work_loops(0) {
        
        if (_num_threads <= 0) {
            _num_threads = static_cast<int>(std::thread::hardware_concurrency());
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
        tasks_processed.store(0, std::memory_order_relaxed);
        tasks_created.store(0, std::memory_order_relaxed);
        idle_threads.store(0, std::memory_order_relaxed);
        outstanding_tasks.store(1, std::memory_order_relaxed);
        total_idle_loops.store(0, std::memory_order_relaxed);
        total_work_loops.store(0, std::memory_order_relaxed);
        
        
        task_pool.clear();
        
        
        task_pool.push(root_task);
        tasks_created.store(1, std::memory_order_relaxed);

        startTimer();

        for (int i = 0; i < _num_threads; ++i) {
            workers.emplace_back(&ParallelTaskRunner::worker_function, this, i);
        }

        {
            std::lock_guard<std::mutex> lk(cv_m);
            cv.notify_all();
        }

        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }
        workers.clear();

        stopTimer();
        
        std::cout << "All threads finished. Processed " << tasks_processed.load() 
                  << " tasks, created " << tasks_created.load() << " tasks.\n";
        std::cout << "Idle loops: " << total_idle_loops.load() 
              << ", Work loops: " << total_work_loops.load() << "\n";
    }

    void stop() {
        termination_requested.store(true, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lk(cv_m);
            cv.notify_all();
        }
        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }
        workers.clear();
    }

    int getTasksProcessed() const { return tasks_processed.load(); }
    int getTasksCreated() const { return tasks_created.load(); }
    int getActiveWorkers() const { return active_workers.load(); }
    int getTotalIdleLoops() const { return total_idle_loops.load(); }
    int getTotalWorkLoops() const { return total_work_loops.load(); }
    
    
   
    /*
    float getParallelEfficiency() const {
        double seq_time = total_time;  // ERROR: total_time doesn't exist!
        double parallel_time = duration();
        if (parallel_time <= 0) return 0.0f;
        return static_cast<float>(seq_time / parallel_time);
    }
    */
};

#endif // PARALLEL_TASK_RUNNER_HPP
