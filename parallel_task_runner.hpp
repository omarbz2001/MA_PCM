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

    std::atomic<bool> termination_requested{false};
    std::atomic<int> active_workers{0};
    std::atomic<int> tasks_processed{0};
    std::atomic<int> tasks_created{0};

    // number of outstanding logical tasks (root counts as 1)
    std::atomic<int> outstanding_tasks{0};

    // mutex ONLY for condition variable
    std::mutex cv_m;
    std::condition_variable cv;

    int _num_threads;

    void worker_function(int /*thread_id*/) {
        active_workers.fetch_add(1, std::memory_order_relaxed);

        while (true) {
            Task* task = task_pool.pop();

            if (!task) {
                // no immediate work → sleep
                std::unique_lock<std::mutex> lk(cv_m);
                cv.wait(lk, [&]() {
                    return termination_requested.load(std::memory_order_relaxed)
                        || !task_pool.empty()
                        || outstanding_tasks.load(std::memory_order_acquire) == 0;
                });

                if (termination_requested.load(std::memory_order_relaxed))
                    break;

                if (outstanding_tasks.load(std::memory_order_acquire) == 0 &&
                    task_pool.empty()) {
                    break;
                }

                task = task_pool.pop();
                if (!task)
                    continue;
            }

            // process task
            int n = task->split(&task_pool);

            if (n > 0) {
                tasks_created.fetch_add(n, std::memory_order_relaxed);
                outstanding_tasks.fetch_add(n, std::memory_order_relaxed);
                delete task;
            } else {
                task->solve();
                delete task;
                tasks_processed.fetch_add(1, std::memory_order_relaxed);
            }

            int remaining =
                outstanding_tasks.fetch_sub(1, std::memory_order_acq_rel) - 1;

            // notify sleepers
            std::lock_guard<std::mutex> lk(cv_m);
            if (remaining == 0)
                cv.notify_all();
            else
                cv.notify_one();

            if (termination_requested.load(std::memory_order_relaxed))
                break;
        }

        active_workers.fetch_sub(1, std::memory_order_relaxed);
    }

public:
    ParallelTaskRunner(int num_threads) : _num_threads(num_threads) {
        if (_num_threads <= 0) {
            _num_threads = static_cast<int>(std::thread::hardware_concurrency());
            if (_num_threads == 0) _num_threads = 4;
        }
        workers.reserve(_num_threads);
    }

    ~ParallelTaskRunner() override {
        stop();
    }

    void run(Task* root_task) override {
        if (!root_task) return;

        termination_requested.store(false, std::memory_order_relaxed);
        tasks_processed.store(0, std::memory_order_relaxed);
        tasks_created.store(0, std::memory_order_relaxed);

        outstanding_tasks.store(1, std::memory_order_relaxed);

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

        std::cout << "All threads finished. Processed "
                  << tasks_processed.load()
                  << " tasks, created "
                  << tasks_created.load() << " tasks.\n";
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
};

#endif // PARALLEL_TASK_RUNNER_HPP
