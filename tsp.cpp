#include <iostream>
#include "tsptask.hpp"

int main(int argc, char** argv) {
	if (argc < 2 || argc > 3) {
		std::cerr << "Usage: " << argv[0] << " <file.tsp> [number]\n";
		return 1;
	}

	TSPGraph graph(argv[1]);
	if (argc == 3)
		graph.resize(atoi(argv[2]));

	TSPPath::setup(&graph);

	TSPTask tsp2(0);
	DirectTaskRunner r2;
	r2.run(&tsp2);
	std::cout << "direct: " << tsp2.result() << " t:" << r2.duration() << std::endl;

	TSPTask tsp1(0);
	PartitionedTaskStackRunner r1(TSPPath::MAX_GRAPH);
	r1.run(&tsp1);
	std::cout << "partit: " << tsp1.result() << " t:" << r1.duration()
		  << " r:" << r1.solveRatio() << std::endl;

	return 0;
}
