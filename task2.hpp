#ifndef TASK2_HPP
#define TASK2_HPP

#include <iostream>

/*****************************************************************
  Task2 class
  Methods:
    split(c) splits a task into subtasks, all added to c
    merge(c) merge results from all subtasks in c
    solve() solves the task without splitting
 *****************************************************************/

class TaskCollection2;

class Task2 {
public:
	virtual int split(TaskCollection2* collection) = 0;
	virtual void merge(TaskCollection2* collection) = 0;
	virtual void solve() = 0;
	virtual void write(std::ostream& os) const = 0;
	virtual ~Task2() = default;
};

std::ostream& operator<<(std::ostream& os, const Task2& t) {
	t.write(os);
	return os;
}

/*****************************************************************
  TaskCollection2 classes
  Methods:
    [i] gets ith element in collection
    push(e) pushes element e at the end of collection
    pop() removes last element added
    clear() removes all elements
    size() gets number of elements in collection
 *****************************************************************/

class TaskCollection2 {
public:
	virtual int size() const = 0;
	virtual Task2* operator[](int i) = 0;
	virtual void push(Task2* t) = 0;
	virtual Task2* pop() = 0;
	virtual void clear() = 0;
	virtual ~TaskCollection2() = default;
};

class TaskStack2 : public TaskCollection2 {
private:
	std::vector<Task2*> _tab;
public:
	TaskStack2(int cap) { _tab.reserve(cap); }
	int size() const override { return _tab.size(); }
	Task2* operator[](int i) override { return _tab[i]; }
	void push(Task2* t) override {
		_tab.push_back(t);
	}
	Task2* pop() override {
		if (_tab.size() <= 0)
			throw std::runtime_error("TaskStack2 empty!");
		Task2* ret = _tab.back();
		_tab.pop_back();
		return ret;
	}
	void clear() override { _tab.clear(); }
};

class FixedTaskStack2 : public TaskCollection2 {
private:
	Task2** _tab;
	int _size;
	int _capacity;
public:
	FixedTaskStack2(Task2** tab, int cap) : _capacity(cap), _tab(tab), _size(0) {}
	int size() const override { return _size; }
	Task2* operator[](int i) override { return _tab[i]; }
	void push(Task2* t) override {
		if (_size >= _capacity)
			throw std::runtime_error("FixedTaskStack2 full!");
		_tab[_size ++] = t;
	}
	Task2* pop() override {
		if (_size <= 0)
			throw std::runtime_error("FixedTaskStack2 empty!");
		return _tab[-- _size];
	}
	void clear() override { _size = 0; }
};

/*****************************************************************
  TaskRunner2 classes
  Methods:
    startTimer() starts mesuring time
    stopTimer() stops measuring time
    duration() gets time between startTimer() and stopTimer()
    run(t) executes task t, must call startTimer() and stopTimer()
 *****************************************************************/

class TaskRunner2 {
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> _start, _stop;
public:
	virtual void run(Task2* t) = 0;
	virtual ~TaskRunner2() = default;
	double duration() const {
		std::chrono::duration<double> diff = _stop - _start;
		return diff.count();   // seconds as a double
	}
protected:
	void startTimer() { _start = std::chrono::high_resolution_clock::now(); }
	void stopTimer() { _stop = std::chrono::high_resolution_clock::now(); }
};

class DirectTaskRunner2 : public TaskRunner2 {
public:
	virtual void run(Task2* t) override {
		TaskRunner2::startTimer();
		t->solve();
		TaskRunner2::stopTimer();
	}
};

#endif
