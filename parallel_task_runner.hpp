#ifndef PARALLEL_TASK_RUNNER_HPP
#define PARALLEL_TASK_RUNNER_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <iostream>
#include "lockfree_stack.hpp"

class ParallelTaskRunner : public TaskRunner {
private:
    LockFreeStack task_pool;
    std::vector<std::thread> workers;
    std::atomic<bool> termination_requested;
    std::atomic<int> active_workers;
    std::atomic<int> tasks_processed;
    std::atomic<int> tasks_created;
    std::atomic<int> idle_threads;
    
    int _num_threads;
    
    void worker_function(int thread_id) {
        active_workers.fetch_add(1, std::memory_order_relaxed);
        
        int idle_loops = 0;
        const int MAX_IDLE_LOOPS = 1000; 
        
        while (!termination_requested.load(std::memory_order_relaxed)) {
            Task* task = task_pool.pop();
            
            if (task == nullptr) {
                idle_loops++;
                idle_threads.fetch_add(1, std::memory_order_relaxed);
                
                
                if (idle_threads.load() >= _num_threads && task_pool.empty()) {
                    
                    idle_threads.fetch_sub(1, std::memory_order_relaxed);
                    break;
                }
                
               
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                idle_threads.fetch_sub(1, std::memory_order_relaxed);
                
                if (idle_loops > MAX_IDLE_LOOPS) {
                    
                    break;
                }
                
                std::this_thread::yield();
                continue;
            }
            
            idle_loops = 0;  
            idle_threads.store(0, std::memory_order_relaxed);  
            
           
            int n = task->split(&task_pool);
            tasks_created.fetch_add(n, std::memory_order_relaxed);
            
            if (n > 0) {
                
                task->merge(&task_pool);
                
                delete task;
            } else {
                
                task->solve();
                delete task;
            }
            
            tasks_processed.fetch_add(1, std::memory_order_relaxed);
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
          idle_threads(0) {
        
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
        
        termination_requested.store(false, std::memory_order_relaxed);
        tasks_processed.store(0, std::memory_order_relaxed);
        tasks_created.store(0, std::memory_order_relaxed);
        idle_threads.store(0, std::memory_order_relaxed);
        
        
        task_pool.clear();
        
        
        std::cout << "Pushing root task to pool\n";
        task_pool.push(root_task);
        tasks_created.store(1, std::memory_order_relaxed);
        
        
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
        
        std::cout << "All threads finished. Processed " << tasks_processed.load() 
                  << " tasks, created " << tasks_created.load() << " tasks.\n";
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
    
    
    int getTasksProcessed() const { return tasks_processed.load(); }
    int getTasksCreated() const { return tasks_created.load(); }
    int getActiveWorkers() const { return active_workers.load(); }
    
    
   
    /*
    float getParallelEfficiency() const {
        double seq_time = total_time;  // ERROR: total_time doesn't exist!
        double parallel_time = duration();
        if (parallel_time <= 0) return 0.0f;
        return static_cast<float>(seq_time / parallel_time);
    }
    */
};

#endif 