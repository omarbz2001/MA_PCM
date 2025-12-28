CPPFLAGS=-O3 -std=c++11 -pthread -march=native

TARGETS=comparator_seq_vs_parallel comparator_cutoff comparator_combined _simpler_implem_tester

all: $(TARGETS)

comparator_seq_vs_parallel: comparator_seq_vs_parallel.cpp tsptask.hpp parallel_task_runner.hpp lockfree_stack.hpp task.hpp tspgraph.hpp
	$(CXX) $(CPPFLAGS) -o comparator_seq_vs_parallel comparator_seq_vs_parallel.cpp

comparator_cutoff: comparator_cutoff.cpp tsptask.hpp parallel_task_runner.hpp task.hpp tspgraph.hpp
	$(CXX) $(CPPFLAGS) -o comparator_cutoff comparator_cutoff.cpp

comparator_combined: comparator_combined.cpp tsptask.hpp parallel_task_runner.hpp task.hpp tspgraph.hpp
	$(CXX) $(CPPFLAGS) -o comparator_combined comparator_combined.cpp

_simpler_implem_tester: _simpler_implem_tester.cpp tsptask.hpp parallel_task_runner.hpp
	$(CXX) $(CPPFLAGS) -o _simpler_implem_tester _simpler_implem_tester.cpp

clean:
	rm -f $(TARGETS)
	rm -f *.o

.PHONY: all clean