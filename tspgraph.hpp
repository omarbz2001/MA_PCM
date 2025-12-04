#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <iomanip>

class TSPGraph {
private:
	struct Point { double x, y; };
	std::vector<Point> _coords;
	std::vector<std::vector<int>> _dist;
	int _width;
	std::string _filename;

public:
	int size() const { return _coords.size(); }
	int distance(int a, int b) const { return _dist[a][b]; }
	void resize(int size) { _coords.resize(size); }

	TSPGraph(const std::string& filename) {
		std::ifstream in(filename);
		if (!in)
			throw std::runtime_error("Cannot open file: " + filename);
		_filename = filename;
		std::string line;
		int dimension = -1;
		bool inCoordSection = false;
		while (std::getline(in, line)) {
			if (line.find("DIMENSION") != std::string::npos) {
				std::stringstream ss(line);
				std::string tmp;
				ss >> tmp; // "DIMENSION", skip until number
				while (ss && !std::isdigit(ss.peek())) ss.get();
				ss >> dimension;
			}
			if (line.find("NODE_COORD_SECTION") != std::string::npos) {
				inCoordSection = true;
				break;
			}
		}
		if (dimension <= 0)
			throw std::runtime_error("Invalid or missing DIMENSION");
		if (!inCoordSection)
			throw std::runtime_error("Missing NODE_COORD_SECTION");
		_coords.assign(dimension, {0,0});
		int count = 0;
		while (std::getline(in, line)) {
			if (line == "EOF") break;
			if (line.empty()) continue;
			std::stringstream ss(line);
			int index;
			double x, y;
			if (!(ss >> index >> x >> y)) continue;
			if (index < 1 || index > dimension)
				throw std::runtime_error("Invalid city index");
			_coords[index - 1] = {x, y};
			count ++;
		}
		if (count != dimension)
			throw std::runtime_error("Coordinate count mismatch");
		_dist.assign(dimension, std::vector<int>(dimension, 0));
		int max = 0;
		for (int i = 0; i < dimension; ++i) {
			for (int j = i + 1; j < dimension; ++j) {
				int d = _dist[j][i] = euc2d(_coords[i], _coords[j]);
				_dist[i][j] = d;
				if (d > max) max = d;
			}
		}
		int digits = 1;
		while (max >= 10) { max /= 10; digits++; }
		_width = digits + 1; // used only for printing
	}

	void write(std::ostream& os) const {
		std::cout << "TSP graph from file " << _filename << '\n';
		int n = size();
		for (int i=0; i<n; i++)
			os << " point " << i << " { x: " << _coords[i].x << ", y: " << _coords[i].y << "}\n";
		os<< "  ";
		for (int j = n-1; j > 0; --j)
			os << std::setw(_width) << j;
		os << '\n';
		for (int i = 0; i < (n-1); i++) {
			os << std::setw(3) << i;
			for (int j = (n-1); j > i; j--)
				os << std::setw(_width) << _dist[i][j];
			os << '\n';
		}
	}

private:
	static int euc2d(const Point& a, const Point& b) {
		double dx = a.x - b.x;
		double dy = a.y - b.y;
		return static_cast<int>(std::round(std::sqrt(dx*dx + dy*dy)));
	}
};

std::ostream& operator<<(std::ostream& os, const TSPGraph& t)
{
	t.write(os);
	return os;
}
