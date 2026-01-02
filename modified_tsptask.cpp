#include "modified_tsptask.hpp"
#include "work_stealing_deque.hpp"
#include <climits>
#include <mutex>
#include <atomic>
#include <algorithm>

// Static member definitions
std::atomic<int> ModifiedTSPTask::best_distance{INT_MAX};
std::atomic<bool> ModifiedTSPTask::initial_bound_set{false};
TSPPath ModifiedTSPTask::best_path;
std::mutex ModifiedTSPTask::best_path_mutex;
int ModifiedTSPTask::_cutoff_size = INT_MAX;


TSPGraph* TSPPath::_graph = nullptr;

ModifiedTSPTask::ModifiedTSPTask(int cutoff)
    : _local_best_check_counter(0)
{
    best_distance.store(INT_MAX, std::memory_order_relaxed);
    initial_bound_set.store(false, std::memory_order_relaxed);
    best_path.maximise();
    _cutoff_size = TSPPath::full() - cutoff;
}

TSPPath ModifiedTSPTask::result() {
    std::lock_guard<std::mutex> lock(best_path_mutex);
    return best_path;
}

bool ModifiedTSPTask::updateBestPath(const TSPPath& candidate) {
    int candidate_dist = candidate.distance();
    int current_best = best_distance.load(std::memory_order_relaxed);
    
    if (candidate_dist < current_best) {
        std::lock_guard<std::mutex> lock(best_path_mutex);
        if (candidate_dist < best_distance.load(std::memory_order_relaxed)) {
            best_path = candidate;
            best_distance.store(candidate_dist, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

bool ModifiedTSPTask::shouldPrune() const {
    return _path.distance() >= best_distance.load(std::memory_order_relaxed);
}

int ModifiedTSPTask::estimateLowerBound() const {
    
    return _path.distance();
}

int ModifiedTSPTask::split(TaskCollection* collection) {
    if (_path.size() >= _cutoff_size) {
        return 0; 
    }
    
    if (shouldPrune()) {
        return 0;
    }
    
    int created = 0;
    for (int i = 1; i < TSPPath::full(); ++i) {
        if (!_path.contains(i)) {
            ModifiedTSPTask* child = new ModifiedTSPTask(_path, i);
            if (!child->shouldPrune()) {
                collection->push(child);
                created++;
            } else {
                delete child;
            }
        }
    }
    return created;
}

void ModifiedTSPTask::solve() {
    if (_path.size() == TSPPath::full()) {
       
        _path.push(TSPPath::FIRST_NODE);
        updateBestPath(_path);
        return;
    }
    
    if (shouldPrune()) {
        return;
    }
    
    
    for (int i = 1; i < TSPPath::full(); ++i) {
        if (!_path.contains(i)) {
            _path.push(i);
            solve();
            _path.pop();
        }
    }
}

void ModifiedTSPTask::write(std::ostream& os) const {
    os << "ModifiedTSPTask: " << _path;
}