CXX = g++
CPPFLAGS = -O3 -std=c++11 -pthread -march=native

TARGETS=comparator_seq_vs_parallel comparator_cutoff comparator_combined _simpler_implem_tester

MODIFIED_TSPSRC = modified_tsptask.cpp

all: $(TARGETS)

comparator_seq_vs_parallel: comparator_seq_vs_parallel.cpp $(MODIFIED_TSPSRC) parallel_task_runner.hpp tspgraph.hpp modified_tsptask.hpp
	$(CXX) $(CPPFLAGS) -o comparator_seq_vs_parallel comparator_seq_vs_parallel.cpp $(MODIFIED_TSPSRC)

comparator_cutoff: comparator_cutoff.cpp $(MODIFIED_TSPSRC) parallel_task_runner.hpp tspgraph.hpp modified_tsptask.hpp
	$(CXX) $(CPPFLAGS) -o comparator_cutoff comparator_cutoff.cpp $(MODIFIED_TSPSRC)

comparator_combined: comparator_combined.cpp $(MODIFIED_TSPSRC) parallel_task_runner.hpp tspgraph.hpp modified_tsptask.hpp
	$(CXX) $(CPPFLAGS) -o comparator_combined comparator_combined.cpp $(MODIFIED_TSPSRC)

_simpler_implem_tester: _simpler_implem_tester.cpp $(MODIFIED_TSPSRC) parallel_task_runner.hpp tspgraph.hpp modified_tsptask.hpp
	$(CXX) $(CPPFLAGS) -o _simpler_implem_tester _simpler_implem_tester.cpp $(MODIFIED_TSPSRC)

clean:
	rm -f $(TARGETS)
	rm -f *.o

.PHONY: all clean
