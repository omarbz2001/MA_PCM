#include <bitset>
#include <climits>

#include "tspgraph.hpp"
#include "task.hpp"


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
	static int full() { return _graph->size(); } // the size of a full path

	TSPPath() {
		_node[0] = FIRST_NODE;
		_size = 1;
		_distance = 0;
		_contents.reset();
		_contents.set(FIRST_NODE);
	}

	void maximise() { _distance = INT_MAX; }
	int size() { return _size; }
	int distance() { return _distance; }
	bool contains(int i) { return _contents.test(i); }
	int tail() { return _node[_size-1]; }

	void push(int node) {
		if (node >= _graph->size())
			throw std::runtime_error("Node outside graph.");
		_distance += _graph->distance(tail(), node);
		_contents.set(node);
		_node[_size ++] = node;
	}

	void pop() {
		if (_size < 2)
			throw std::runtime_error("Empty path to pop().");
		_size --;
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
};

std::ostream& operator<<(std::ostream& os, const TSPPath& t) {
	t.write(os);
	return os;
}

class TSPTask : public Task {

private:
	static int _cutoff_size;
	static TSPPath _shortest;
	static std::vector<TSPTask*> _free_list;

	// this does not work with multiple threads!	
	static TSPTask* alloc(const TSPPath& path, int node) {
		if (_free_list.empty())
			return new TSPTask(path, node);
		TSPTask* p = _free_list.back();
		_free_list.pop_back();
		p->_path = path;
		p->_path.push(node);
		return p;
        }

        // this does not work with multiple threads!
        static void free(TSPTask* p) {
//        	delete p;
        	_free_list.push_back(p);
        }

        TSPPath _path;

	TSPTask() { throw std::runtime_error("Cannot construct TSPTask(void)"); }

	TSPTask(const TSPPath& path, int node) : _path(path) {
		_path.push(node);
	}

public:
	TSPTask(int cutoff) { // cutoff expressed as a distance from full
		_shortest.maximise();
		_cutoff_size = TSPPath::full() - cutoff;
	}
	~TSPTask() override = default;

	int size() {
		return TSPPath::full();
	}

	TSPPath& result() {
		return _shortest;
	}

	// Task interface implementation: split, merge, solve, write

	int split(TaskCollection* collection) override {
		collection->clear();
		if (_path.size() >= _cutoff_size) return 0;
		int count = 0;
		for (int i=0; i<TSPPath::full(); i++) {
			if (!_path.contains(i)){
//				TSPTask* t  = new TSPTask(_path, i);
				TSPTask* t = TSPTask::alloc(_path, i);
				collection->push(t);
				count ++;
			}
		}
		return count;
	}

	void merge(TaskCollection* collection) override {
		for (int p=0; p<collection->size(); p++) {
			TSPTask* t = (TSPTask*) collection->pop();
//			delete t;
			TSPTask::free(t);
		}
	}

	void solve() override {
//		std::cout << "solving " << _path << "\n";
		if (_path.size() == TSPPath::full()) {
			_path.push(TSPPath::FIRST_NODE); // last node = first node
			if (_path.distance() < _shortest.distance())
				_shortest = _path;
			_path.pop();
		} else {
			for (int i=0; i<TSPPath::full(); i++) {
				if (!_path.contains(i)) {
					_path.push(i);
					if (_path.distance() < _shortest.distance())
						solve();
					_path.pop();
				}
			}
		}
	}

	void write(std::ostream& os) const override {
		std::cout << "Task" << _path;
	}
};

TSPGraph* TSPPath::_graph;
int TSPTask::_cutoff_size = INT_MAX;
TSPPath TSPTask::_shortest;
std::vector<TSPTask*> TSPTask::_free_list;
