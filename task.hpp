#ifndef TASK_HPP
#define TASK_HPP

#include <iostream>
#include <chrono>

// Forward declaration
class TaskCollection;

class Task {
public:
    virtual ~Task() = default;
    virtual int split(TaskCollection* collection) = 0;
    virtual void merge(TaskCollection* collection) = 0;
    virtual void solve() = 0;
    virtual void write(std::ostream& os) const = 0;
};

class TaskCollection {
public:
    virtual ~TaskCollection() = default;
    virtual int size() const = 0;
    virtual Task* operator[](int index) = 0;
    virtual void push(Task* t) = 0;
    virtual Task* pop() = 0;
    virtual void clear() = 0;
};

class TaskRunner {
protected:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
    
public:
    virtual ~TaskRunner() = default;
    virtual void run(Task* task) = 0;
    
    void startTimer() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    void stopTimer() {
        end_time = std::chrono::high_resolution_clock::now();
    }
    
    double duration() const {
        std::chrono::duration<double> diff = end_time - start_time;
        return diff.count();
    }
};

class DirectTaskRunner : public TaskRunner {
public:
    void run(Task* task) override {
        if (!task) return;
        startTimer();
        task->solve();
        stopTimer();
    }
};

// Inline operator<< definition (only one definition in the file)
inline std::ostream& operator<<(std::ostream& os, const Task& t) {
    t.write(os);
    return os;
}

#endif // TASK_HPP