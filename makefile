CPPFLAGS=-O3 -std=c++11 -pthread -march=native
#CPPFLAGS=-g -std=c++11 -pthread -Wall -Wextra
#CPPFLAGS=-std=c++20 -pthread

# Original targets
TARGETS=tsp tspprint intvecsort

# New parallel target
PARALLEL_TARGETS=parallel_tsp

# All targets including parallel
ALL_TARGETS=$(TARGETS) $(PARALLEL_TARGETS)

all: $(ALL_TARGETS)

# Original programs
tsp: tsp.cpp tsptask.hpp task.hpp tspgraph.hpp
	$(CXX) $(CPPFLAGS) -o tsp tsp.cpp

tspprint: tspprint.cpp tspgraph.hpp
	$(CXX) $(CPPFLAGS) -o tspprint tspprint.cpp

intvecsort: intvecsort.cpp intvecsorttask.hpp
	$(CXX) $(CPPFLAGS) -o intvecsort intvecsort.cpp

# Parallel TSP program
parallel_tsp: parallel_tsp.cpp modified_tsptask.hpp parallel_task_runner.hpp lockfree_stack.hpp task.hpp tspgraph.hpp
	$(CXX) $(CPPFLAGS) -o parallel_tsp parallel_tsp.cpp






# Performance test with different thread counts
perf_test: parallel_tsp
	@echo "Performance scaling test..."
	@for threads in 1 2 4 8 16 32; do \
		echo -n "Threads=$$threads: "; \
		timeout 30 ./parallel_tsp test_data/example.tsp 12 $$threads 2>/dev/null | grep "Speedup:" || echo "Timeout or error"; \
	done

# Clean everything
clean:
	rm -f $(ALL_TARGETS)
	rm -f *.o


.PHONY: all clean test_small test_medium perf_test test_data