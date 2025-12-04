#include <iostream>
#include "intvecsorttask.hpp"

int main()
{
	IntVecSortTask iv1;
	iv1.randomize(100);
	IntVecSortTask iv2 = iv1;

	DirectTaskRunner sr;
	sr.run(&iv1);
	std::cout << "direct:" << iv1 << " t:" << sr.duration() << std::endl;

	PartitionedTaskStackRunner rr(2);
	rr.run(&iv2);
	std::cout << "partit:" << iv2 << " t:" << rr.duration()
		<<  " r:" << rr.solveRatio() << std::endl;
}
