#ifndef PARALLEL_TASK_RUNNER_HPP
#define PARALLEL_TASK_RUNNER_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include <memory>
#include "task.hpp"
#include "work_stealing_deque.hpp"
#include "modified_tsptask.hpp"

class ParallelTaskRunner : public TaskRunner {
private:
    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<WorkStealingDeque>> deques;

    std::atomic<bool> termination_requested;
    std::atomic<int> active_tasks;

    int _num_threads;

    void worker_function(int id) {
        WorkStealingDeque& local = *deques[id];

        while (!termination_requested.load(std::memory_order_relaxed)) {
            Task* task = local.pop();

            if (!task) {
                for (int i = 0; i < _num_threads; ++i) {
                    if (i == id) continue;
                    task = deques[i]->steal();
                    if (task) break;
                }
            }

            if (!task) {
                if (active_tasks.load(std::memory_order_acquire) == 0)
                    break;
                std::this_thread::yield();
                continue;
            }

            ModifiedTSPTask* tsp = static_cast<ModifiedTSPTask*>(task);
            int created = tsp->split(&local);

            if (created == 0) {
                tsp->solve();
            }

            active_tasks.fetch_add(created - 1, std::memory_order_relaxed); // +created tasks, -1 finished
            delete tsp;
        }
    }

public:
    explicit ParallelTaskRunner(int num_threads)
        : termination_requested(false),
          active_tasks(0),
          _num_threads(num_threads) {

        if (_num_threads <= 0) {
            _num_threads = std::thread::hardware_concurrency();
            if (_num_threads == 0) _num_threads = 4;
        }

        deques.reserve(_num_threads);
        for (int i = 0; i < _num_threads; ++i)
            deques.push_back(std::unique_ptr<WorkStealingDeque>(new WorkStealingDeque()));

        workers.reserve(_num_threads);
    }

    ~ParallelTaskRunner() override {
        stop();
    }

    void run(Task*) override {
        throw std::runtime_error("Use run(ModifiedTSPTask*) instead");
    }

    void run(ModifiedTSPTask* root_task) {
        if (!root_task) return;

        termination_requested.store(false, std::memory_order_relaxed);

        for (auto& d : deques)
            d->clear();

        deques[0]->push(root_task);
        active_tasks.store(1, std::memory_order_relaxed);

        startTimer();

        std::cout << "Launching " << _num_threads << " worker threads\n";

        for (int i = 0; i < _num_threads; ++i) {
            workers.emplace_back(&ParallelTaskRunner::worker_function, this, i);
        }

        for (auto& w : workers) {
            if (w.joinable())
                w.join();
        }

        workers.clear();
        stopTimer();

        std::cout << "All threads finished\n";
    }

    void stop() {
        termination_requested.store(true, std::memory_order_relaxed);
        for (auto& w : workers) {
            if (w.joinable())
                w.join();
        }
        workers.clear();
    }
};

#endif // PARALLEL_TASK_RUNNER_HPP
