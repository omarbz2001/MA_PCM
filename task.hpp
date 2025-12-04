#ifndef TASK_HPP
#define TASK_HPP

#include <iostream>
#include <chrono>
#include <vector>  // ADD THIS LINE
#include <stdexcept>  // ADD THIS TOO

class TaskCollection;

class Task {
public:
	virtual int split(TaskCollection* collection) = 0;
	virtual void merge(TaskCollection* collection) = 0;
	virtual void solve() = 0;
	virtual void write(std::ostream& os) const = 0;
	virtual ~Task() = default;
};

std::ostream& operator<<(std::ostream& os, const Task& t) {
	t.write(os);
	return os;
}

class TaskCollection {
public:
	virtual int size() const = 0;
	virtual Task* operator[](int i) = 0;
	virtual void push(Task* t) = 0;
	virtual Task* pop() = 0;
	virtual void clear() = 0;
    virtual ~TaskCollection() = default;  // ADD VIRTUAL DESTRUCTOR
};

class TaskStack : public TaskCollection {
private:
	std::vector<Task*> _tab;
public:
	TaskStack(int cap) { _tab.reserve(cap); }
	int size() const override { return (int)_tab.size(); }
	Task* operator[](int i) override { return _tab[i]; }
	void push(Task* t) override {
		_tab.push_back(t);
	}
	Task* pop() override {
		if (_tab.size() <= 0)
			throw std::runtime_error("TaskStack empty!");
		Task* ret = _tab.back();
		_tab.pop_back();
		return ret;
	}
	void clear() override { _tab.clear(); }
};

class FixedTaskStack : public TaskCollection {
private:
	Task** _tab;
	int _size;
	int _capacity;
public:
	FixedTaskStack(Task** tab, int cap) : _capacity(cap), _tab(tab), _size(0) {}
	int size() const override { return _size; }
	Task* operator[](int i) override { return _tab[i]; }
	void push(Task* t) override {
		if (_size >= _capacity)
			throw std::runtime_error("FixedTaskStack full!");
		_tab[_size ++] = t;
	}
	Task* pop() override {
		if (_size <= 0)
			throw std::runtime_error("FixedTaskStack empty!");
		return _tab[-- _size];
	}
	void clear() override { _size = 0; }
};

class TaskRunner {
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> _start, _stop;
public:
	virtual void run(Task* t) = 0;
	virtual ~TaskRunner() = default;
	double duration() const {
		std::chrono::duration<double> diff = _stop - _start;
		return diff.count();   // seconds as a double
	}
protected:
	void startTimer() { _start = std::chrono::high_resolution_clock::now(); }
	void stopTimer() { _stop = std::chrono::high_resolution_clock::now(); }
};

class DirectTaskRunner : public TaskRunner {
public:
	virtual void run(Task* t) override {
		TaskRunner::startTimer();
		t->solve();
		TaskRunner::stopTimer();
	}
};

class PartitionedTaskStackRunner : public TaskRunner {
private:
	int _size;
	int _splits;
	int _solves;
	void recurse(Task* t) {
		TaskStack coll(_size);
// 		Task* space[_size];
//		FixedTaskStack coll(space, _size);
		int n = t->split(&coll);
		if (n) {
			_splits ++;
			for (int i=0; i<n; i++)
				recurse(coll[i]);
			t->merge(&coll);
		} else {
			_solves ++;
			t->solve();
		}
	}
	PartitionedTaskStackRunner() {} // cannot use default constructor
public:
	PartitionedTaskStackRunner(int size) : _size(size), _splits(0), _solves(0) {}
	virtual void run(Task* t) override {
		TaskRunner::startTimer();
		recurse(t);
		TaskRunner::stopTimer();
	}
	float solveRatio() {
		return _solves / (float) (_solves + _splits);
	}
};

#endif // TASK_HPP  // ADD HEADER GUARD