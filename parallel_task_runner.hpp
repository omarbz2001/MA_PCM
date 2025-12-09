#ifndef PARALLEL_TASK_RUNNER_HPP
#define PARALLEL_TASK_RUNNER_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <iostream>
#include <condition_variable>
#include <mutex>

#include "task.hpp"
#include "lockfree_stack.hpp"

// ParallelTaskRunner: spawns N workers, uses a shared TaskCollection, and
// tracks outstanding tasks via an atomic counter. Workers sleep on CV when idle.

class ParallelTaskRunner : public TaskRunner {
private:
    LockFreeStack task_pool; // thread-safe stack implementing TaskCollection
    std::vector<std::thread> workers;

    std::atomic<bool> termination_requested{false};
    std::atomic<int> active_workers{0};
    std::atomic<int> tasks_processed{0};
    std::atomic<int> tasks_created{0};

    // number of outstanding logical tasks (root counts as 1)
    std::atomic<int> outstanding_tasks{0};

    std::mutex cv_m;
    std::condition_variable cv;

    int _num_threads;

    void worker_function(int /*thread_id*/) {
        active_workers.fetch_add(1, std::memory_order_relaxed);

        while (true) {
            Task* task = nullptr;

            // Try to fetch a task quickly
            task = task_pool.pop();

            if (!task) {
                // no immediate work -> wait on CV
                std::unique_lock<std::mutex> lk(cv_m);
                cv.wait(lk, [&]() {
                    return termination_requested.load(std::memory_order_relaxed)
                        || !task_pool.empty()
                        || outstanding_tasks.load(std::memory_order_acquire) == 0;
                });

                if (termination_requested.load(std::memory_order_relaxed)) break;

                // if there are no outstanding tasks and pool empty -> done
                if (outstanding_tasks.load(std::memory_order_acquire) == 0 && task_pool.empty()) {
                    break;
                }

                // try pop again
                task = task_pool.pop();
                if (!task) {
                    continue;
                }
            }

            // If we have a task, process it
            int n = task->split(&task_pool);

            if (n > 0) {
                // split created n new tasks and pushed them onto the collection
                tasks_created.fetch_add(n, std::memory_order_relaxed);
                // these n tasks increase outstanding count; parent task is considered consumed
                outstanding_tasks.fetch_add(n, std::memory_order_relaxed);
                delete task; // parent task no longer needed
            } else {
                // leaf: solve inline
                task->solve();
                delete task;
                tasks_processed.fetch_add(1, std::memory_order_relaxed);
            }

            // we finished handling one logical task (either solved leaf or spawned children)
            int remaining = outstanding_tasks.fetch_sub(1, std::memory_order_acq_rel) - 1;

            if (remaining == 0) {
                // no more outstanding tasks -> notify all workers to exit
                std::lock_guard<std::mutex> lk(cv_m);
                cv.notify_all();
            } else {
                // wake one worker to continue processing
                std::lock_guard<std::mutex> lk(cv_m);
                cv.notify_one();
            }

            if (termination_requested.load(std::memory_order_relaxed)) break;
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

    virtual void run(Task* root_task) override {
        if (!root_task) return;

        termination_requested.store(false, std::memory_order_relaxed);
        tasks_processed.store(0, std::memory_order_relaxed);
        tasks_created.store(0, std::memory_order_relaxed);

        // outstanding tasks: root counts as 1
        outstanding_tasks.store(1, std::memory_order_relaxed);

        // clear pool and push root
        task_pool.clear();
        task_pool.push(root_task);
        tasks_created.store(1, std::memory_order_relaxed);

        startTimer();

        // spawn workers
        for (int i = 0; i < _num_threads; ++i) {
            workers.emplace_back(&ParallelTaskRunner::worker_function, this, i);
        }

        // notify workers that work is available
        {
            std::lock_guard<std::mutex> lk(cv_m);
            cv.notify_all();
        }

        // join workers
        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
        workers.clear();

        stopTimer();

        std::cout << "All threads finished. Processed " << tasks_processed.load()
                  << " tasks, created " << tasks_created.load() << " tasks.\n";
    }

    void stop() {
        termination_requested.store(true, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lk(cv_m);
            cv.notify_all();
        }
        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
        workers.clear();
    }

    int getTasksProcessed() const { return tasks_processed.load(); }
    int getTasksCreated() const { return tasks_created.load(); }
    int getActiveWorkers() const { return active_workers.load(); }
};

#endif // PARALLEL_TASK_RUNNER_HPP
