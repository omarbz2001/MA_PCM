#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include "modified_tsptask.hpp"
#include "parallel_task_runner.hpp"
#include "tsptaskseq.hpp"

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <file.tsp> <num_cities> <num_threads>\n";
        std::cerr << "Example: " << argv[0] << " example.tsp 10 8\n";
        std::cerr << "Usage: " << argv[0] << " <file.tsp> <num_cities> <num_threads> [cutoff]\n";
        std::cerr << "Example: " << argv[0] << " example.tsp 12 8 3\n";
        return 1;
    }

    std::string filename = argv[1];
    int num_cities = std::atoi(argv[2]);
    int num_threads = std::atoi(argv[3]);
    int cutoff = 0;
    if (argc >= 5) cutoff = std::atoi(argv[4]);

    if (num_threads <= 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
        std::cout << "Using " << num_threads << " threads (auto-detected)\n";
    }

    // Load and setup graph
    std::cout << "Loading TSP file: " << filename << std::endl;
    TSPGraph graph(filename);
    
    if (num_cities > 0 && num_cities < graph.size()) {
        graph.resize(num_cities);
    }
    
    std::cout << "Graph size: " << graph.size() << " cities\n";
    std::cout << "Using " << num_threads << " threads\n";
    std::cout << "Cutoff: " << cutoff << "\n\n";
    
    TSPPath::setup(&graph);
    
    // Create task with chosen cutoff
    ModifiedTSPTask* tsp_task = new ModifiedTSPTask(cutoff);
    
    // Run parallel version
    std::cout << "\nRunning parallel version with " << num_threads << " threads..." << std::endl;
    
    ParallelTaskRunner parallel_runner(num_threads);
    parallel_runner.run(tsp_task);
    
    double parallel_time = parallel_runner.duration();
    std::cout << "Parallel time: " << parallel_time << " seconds\n";
    
    TSPPath result = tsp_task->result();
    std::cout << "Best path found: " << result << std::endl;

    // Run sequential version for comparison
    std::cout << "\nRunning sequential version..." << std::endl;
    TSPPathSeq::setup(&graph);
    TSPTaskSeq* tsp_task_seq = new TSPTaskSeq(cutoff);
    DirectTaskRunner2 sequential_runner;
    sequential_runner.run(tsp_task_seq);
    
    double sequential_time = sequential_runner.duration();
    std::cout << "Sequential time: " << sequential_time << " seconds\n";
    
    TSPPathSeq result_seq = tsp_task_seq->result();
    std::cout << "Best path found (sequential): " << result_seq << std::endl;

    // Compare results
    if (result.distance() == result_seq.distance()) {
        std::cout << "\033[32mResults match!\033[0m\n";
    } else {
        std::cout << "\033[31mResults differ!\033[0m\n";
    }

    // Calculate and display speedup statistics
    double speedup = sequential_time / parallel_time;
    double efficiency = speedup / num_threads * 100.0;
    
    std::cout << "\n--- Performance Statistics ---\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Speedup: " << speedup << "x\n";
    std::cout << "Efficiency: " << efficiency << "%\n";
    std::cout << "Threads used: " << num_threads << "\n";
    
    // Display results
    std::string results_header = "\n****Results:****\n";
    std::cout << results_header;
    
    std::string run_info = "Run Information:\n";
    std::cout << run_info;
    
    std::cout << "Filename: " << filename << "\n";
    
    std::cout << "Number of cities: " << graph.size() << "\n";
    
    std::cout << "Number of threads: " << num_threads << "\n";
    
    std::cout << "Cutoff: " << cutoff << "\n\n";
    
    std::cout << std::fixed << std::setprecision(3);
    
    std::string table_header = "Type\t\tTime(s)\t\tSpeedup\t\tEfficiency(%)\n----\t\t------\t\t-------\t\t------------\n";
    std::cout << table_header;
    
    std::cout << "Parallel\t" << parallel_time << "\t\t" << speedup << "\t\t" << efficiency << "\n";
    
    std::cout << "Sequential\t" << sequential_time << "\t\tN/A\t\tN/A\n";
    
    return 0;
}