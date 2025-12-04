#ifndef INTVECSORTTASK_HPP
#define INTVECSORTTASK_HPP

#include "task.hpp"  
#include <vector>
#include <algorithm>
#include <random>

class IntVecSortTask : public Task {
private:
    std::vector<int> _vec;
    static std::mt19937 _rng;
    
public:
    IntVecSortTask() = default;
    IntVecSortTask(const std::vector<int>& v) : _vec(v) {}
    
    void randomize(int size) {
        _vec.resize(size);
        std::uniform_int_distribution<int> dist(0, 1000);
        for (auto& x : _vec) x = dist(_rng);
    }
    
    int split(TaskCollection* collection) override {
        if (_vec.size() <= 1) return 0;
        int mid = _vec.size() / 2;
        std::vector<int> left(_vec.begin(), _vec.begin() + mid);
        std::vector<int> right(_vec.begin() + mid, _vec.end());
        collection->push(new IntVecSortTask(left));
        collection->push(new IntVecSortTask(right));
        return 2;
    }
    
    void merge(TaskCollection* collection) override {
        if (collection->size() != 2) throw std::runtime_error("Expected 2 subtasks");
        IntVecSortTask* left = dynamic_cast<IntVecSortTask*>(collection->operator[](0));
        IntVecSortTask* right = dynamic_cast<IntVecSortTask*>(collection->operator[](1));
        if (!left || !right) throw std::runtime_error("Invalid task types");
        
        _vec.clear();
        std::merge(left->_vec.begin(), left->_vec.end(),
                  right->_vec.begin(), right->_vec.end(),
                  std::back_inserter(_vec));
        
        delete left;
        delete right;
        collection->clear();
    }
    
    void solve() override {
        std::sort(_vec.begin(), _vec.end());
    }
    
    void write(std::ostream& os) const override {
        os << "[";
        for (size_t i = 0; i < _vec.size(); ++i) {
            if (i > 0) os << ", ";
            os << _vec[i];
        }
        os << "]";
    }
};

std::mt19937 IntVecSortTask::_rng(std::random_device{}());

#endif 