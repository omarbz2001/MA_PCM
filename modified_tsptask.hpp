#ifndef MODIFIED_TSPTASK_HPP
#define MODIFIED_TSPTASK_HPP

#include <bitset>
#include <climits>
#include <atomic>
#include <vector>
#include <mutex>
#include <stdexcept>
#include <ostream>

#include "tspgraph.hpp"
#include "task.hpp"

class TSPPath;

std::ostream& operator<<(std::ostream& os, const TSPPath& t);

class TSPPath {
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

    TSPPath() {
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

    TSPPath& operator=(const TSPPath& other) {
        if (this != &other) {
            _size = other._size;
            _distance = other._distance;
            _contents = other._contents;
            for (int i = 0; i < _size; ++i) _node[i] = other._node[i];
        }
        return *this;
    }
};

inline std::ostream& operator<<(std::ostream& os, const TSPPath& t) {
    t.write(os);
    return os;
}

// ---------------------------------------------------------
// ModifiedTSPTask

class ModifiedTSPTask : public Task {
private:
    static std::atomic<int> best_distance;
    static std::atomic<bool> initial_bound_set;
    static TSPPath best_path;
    static std::mutex best_path_mutex;
    static int _cutoff_size;

    TSPPath _path;
    mutable int _local_best_check_counter;

    ModifiedTSPTask() = delete;

    ModifiedTSPTask(const TSPPath& path, int node)
        : _path(path), _local_best_check_counter(0) {
        _path.push(node);
    }

    static void computeInitialBound();

public:
    ModifiedTSPTask(int cutoff);

    ~ModifiedTSPTask() override = default;

    TSPPath result();

    static bool updateBestPath(const TSPPath& candidate);

    bool shouldPrune() const;
    int estimateLowerBound() const;

    int split(TaskCollection* collection) override;
    void merge(TaskCollection*) override {}
    void solve() override;
    void write(std::ostream& os) const override;
};

#endif // MODIFIED_TSPTASK_HPP