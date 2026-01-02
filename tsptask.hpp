#ifndef TSPTASKSEQ_HPP
#define TSPTASKSEQ_HPP

#include <bitset>
#include <climits>
#include <atomic>
#include <vector>
#include <mutex>
#include <stdexcept>
#include <ostream>

#include "tspgraph.hpp"
#include "task.hpp"
#include "lockfree_stack.hpp"

// Rename the class declaration and operator for the new class name
class TSPPathSeq;

std::ostream& operator<<(std::ostream& os, const TSPPathSeq& t);

class TSPPathSeq {
public:
    static const int FIRST_NODE = 0;
    static const int MAX_GRAPH = 32;
private:
    static TSPGraph* _graph;
    int _node[MAX_GRAPH];
    int _size;
    int _distance;
    std::bitset<MAX_GRAPH> _contents;
public:
    static void setup(TSPGraph *graph) {
        _graph = graph;
        if (_graph->size() > MAX_GRAPH)
            throw std::runtime_error("Graph bigger than MAX_GRAPH");
    }
    static int full() { return _graph->size(); }
    static int graphDistance(int a, int b) { return _graph->distance(a, b); }

    TSPPathSeq() {
        _node[0] = FIRST_NODE;
        _size = 1;
        _distance = 0;
        _contents.reset();
        _contents.set(FIRST_NODE);
    }

    void maximise() { _distance = INT_MAX; }
    int size() const { return _size; }
    int distance() const { return _distance; }
    bool contains(int i) const { return _contents.test(i); }
    int tail() const { return _node[_size-1]; }

    void push(int node) {
        if (node >= _graph->size())
            throw std::runtime_error("Node outside graph.");
        _distance += _graph->distance(tail(), node);
        _contents.set(node);
        _node[_size++] = node;
    }

    void pop() {
        if (_size < 2)
            throw std::runtime_error("Empty path to pop().");
        _size--;
        int oldtail = _node[_size];
        int newtail = _node[_size-1];
        if (oldtail != FIRST_NODE)
            _contents.reset(oldtail);
        _distance -= _graph->distance(newtail, oldtail);
    }

    void write(std::ostream& os) const {
        os << "{" << _distance << ": ";
        for (int i=0; i<_size; i++) {
            if (i) os << ", ";
            os << _node[i];
        }
        os << "}";
    }

    TSPPathSeq& operator=(const TSPPathSeq& other) {
        if (this != &other) {
            _size = other._size;
            _distance = other._distance;
            _contents = other._contents;
            for (int i = 0; i < _size; ++i) _node[i] = other._node[i];
        }
        return *this;
    }
};

// Adjusted operator for the renamed class
inline std::ostream& operator<<(std::ostream& os, const TSPPathSeq& t) {
    t.write(os);
    return os;
}

class TSPTaskSeq : public Task {
private:
    // shared among all tasks
    static std::atomic<int> best_distance;
    static TSPPathSeq best_path;
    static std::mutex best_path_mutex;

    static int _cutoff_size;

    TSPPathSeq _path;

    TSPTaskSeq() { throw std::runtime_error("Cannot construct TSPTaskSeq(void)"); }

    TSPTaskSeq(const TSPPathSeq& path, int node)
        : _path(path) {
        _path.push(node);
    }

public:
    TSPTaskSeq(int cutoff) {
        best_distance.store(INT_MAX, std::memory_order_relaxed);
        best_path.maximise();
        _cutoff_size = TSPPathSeq::full() - cutoff;

        // Compute initial bound: path 0-1-2-...-n-0
        TSPPathSeq initial;
        for (int i = 1; i < TSPPathSeq::full(); ++i) {
            initial.push(i);
        }
        initial.push(TSPPathSeq::FIRST_NODE);
        best_distance.store(initial.distance(), std::memory_order_relaxed);
        best_path = initial;
    }

    ~TSPTaskSeq() override = default;

    TSPPathSeq result() {
        return best_path;
    }

    static bool updateBestPath(const TSPPathSeq& candidate) {
        int candidate_dist = candidate.distance();
        int current_best = best_distance.load(std::memory_order_acquire);

        while (candidate_dist < current_best) {
            if (best_distance.compare_exchange_weak(
                    current_best,
                    candidate_dist,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {

                std::lock_guard<std::mutex> lock(best_path_mutex);
                best_path = candidate;
                return true;
            }
        }
        return false;
    }

    int split(TaskCollection* collection, int& prunedLeaves) {
        if (_path.size() >= _cutoff_size) return 0;

        int keptChildren = 0;
        prunedLeaves = 0;

        int current_best = best_distance.load(std::memory_order_acquire);

        for (int i = 0; i < TSPPathSeq::full(); ++i) {
            if (!_path.contains(i)) {
                int new_dist = _path.distance()
                            + TSPPathSeq::graphDistance(_path.tail(), i);

                TSPTaskSeq* child = new TSPTaskSeq(_path, i);
                int childLeaves = child->remainingLeaves(true);

                if (new_dist < current_best) {
                    collection->push(child);
                    keptChildren++;
                } else {
                    prunedLeaves += childLeaves;
                    delete child;
                }
            }
        }

        return keptChildren;
    }

    int split(TaskCollection* collection) override {
        if (_path.size() >= _cutoff_size) return 0;

        int count = 0;
        int current_best = best_distance.load(std::memory_order_acquire);

        for (int i = 0; i < TSPPathSeq::full(); ++i) {
            if (!_path.contains(i)) {
                int new_dist = _path.distance() + TSPPathSeq::graphDistance(_path.tail(), i);
                if (new_dist < current_best) {
                    TSPTaskSeq* t = new TSPTaskSeq(_path, i);
                    collection->push(t);
                    ++count;
                }
            }
        }
        return count;
    }

    void merge(TaskCollection*) override {}

    void solve() override {
        if (_path.size() == TSPPathSeq::full()) {
            _path.push(TSPPathSeq::FIRST_NODE);
            if (_path.distance() < best_distance.load(std::memory_order_acquire)) {
                updateBestPath(_path);
            }
            _path.pop();
        } else {
            int current_best = best_distance.load(std::memory_order_acquire);
            for (int i = 0; i < TSPPathSeq::full(); ++i) {
                if (!_path.contains(i)) {
                    int new_dist = _path.distance() + TSPPathSeq::graphDistance(_path.tail(), i);
                    if (new_dist < current_best) {
                        _path.push(i);
                        solve();
                        _path.pop();
                        current_best = best_distance.load(std::memory_order_acquire);
                    }
                }
            }
        }
    }

    void write(std::ostream& os) const override {
        os << "Task" << _path;
    }

    int remainingLeaves(bool parentIncluded = false) const {
        int maxDepth = _cutoff_size - _path.size();
        int n = TSPPathSeq::full() - _path.size();

        int sum = 0;
        int perm = 1;

        for (int k = 1; k <= maxDepth && k <= n; ++k) {
            perm *= (n - k + 1); // P(n,k)
            sum += perm;
        }

        return parentIncluded ? sum + 1 : sum;
    }
};

// static definitions
TSPGraph* TSPPathSeq::_graph = nullptr;
std::atomic<int> TSPTaskSeq::best_distance{INT_MAX};
TSPPathSeq TSPTaskSeq::best_path;
std::mutex TSPTaskSeq::best_path_mutex;
int TSPTaskSeq::_cutoff_size = INT_MAX;

#endif // TSPTASKSEQ_HPP
