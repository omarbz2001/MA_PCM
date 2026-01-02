#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <fstream>
#include <string>
#include "modified_tsptask.hpp"
#include "parallel_task_runner.hpp"
#include "tsptaskseq.hpp"


int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <file.tsp> <num_cities> <num_threads> [cutoff1 cutoff2 ...]\n";
        std::cerr << "Example: " << argv[0] << " example.tsp 10 8 2 3 4\n";
        return 1;
    }

    std::string filename = argv[1];
    int num_cities = std::atoi(argv[2]);
    int num_threads = std::atoi(argv[3]);

    std::vector<int> cutoffs;
    if (argc >= 5) {
        for (int i = 4; i < argc; i++) {
            cutoffs.push_back(std::atoi(argv[i]));
        }
    } else {
        cutoffs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}; // Default cutoffs to test
    }

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
    std::cout << "Testing cutoffs: ";
    for (int c : cutoffs) std::cout << c << " ";
    std::cout << "\n\n";

    TSPPathSeq::setup(&graph);

    // Run sequential once with cutoff 0
    std::cout << "Running sequential TSP with cutoff 0...\n";
    TSPTaskSeq* tsp_task_seq = new TSPTaskSeq(0);
    DirectTaskRunner2 sequential_runner;
    sequential_runner.run(tsp_task_seq);
    double sequential_time = sequential_runner.duration();
    std::cout << "Sequential time: " << sequential_time << " seconds\n\n";

    // Compare each cutoff for parallel
    struct Result {
        int cutoff;
        double parallel_time;
        double speedup;
        double efficiency;
    };
    std::vector<Result> results;

    for (int cutoff : cutoffs) {
        std::cout << "Running parallel TSP with cutoff " << cutoff << "...\n";
        TSPPath::setup(&graph);
        ModifiedTSPTask* tsp_task_par = new ModifiedTSPTask(cutoff);
        ParallelTaskRunner parallel_runner(num_threads);
        parallel_runner.run(tsp_task_par);
        double parallel_time = parallel_runner.duration();

        double speedup = sequential_time / parallel_time;
        double efficiency = speedup / num_threads * 100.0;

        results.push_back({cutoff, parallel_time, speedup, efficiency});

        std::cout << "Cutoff " << cutoff << ": Parallel=" << parallel_time << "s, Speedup=" << speedup << "x\n";
    }

    // Create results directory and file
    std::string base_name = filename.substr(0, filename.find_last_of('.'));
    std::string file_path = "results/combined/" + base_name + "_" + std::to_string(graph.size()) + "_" + std::to_string(num_threads) + ".txt";
    std::ofstream out(file_path);

    // Display results
    std::string results_header = "\n****Results:****\n";
    std::cout << results_header;
    out << results_header;

    std::string run_info = "Run Information:\n";
    std::cout << run_info;
    out << run_info;

    std::cout << "Filename: " << filename << "\n";
    out << "Filename: " << filename << "\n";

    std::cout << "Number of cities: " << graph.size() << "\n";
    out << "Number of cities: " << graph.size() << "\n";

    std::cout << "Number of threads: " << num_threads << "\n";
    out << "Number of threads: " << num_threads << "\n";

    std::cout << "Cutoffs tested: ";
    out << "Cutoffs tested: ";
    for (size_t i = 0; i < cutoffs.size(); ++i) {
        std::cout << cutoffs[i];
        out << cutoffs[i];
        if (i < cutoffs.size() - 1) {
            std::cout << ", ";
            out << ", ";
        }
    }
    std::cout << "\n\n";
    out << "\n\n";

    std::cout << std::fixed << std::setprecision(3);
    out << std::fixed << std::setprecision(3);

    std::string table_header = "Cutoff\t\tType\t\tTime(s)\t\tSpeedup\t\tEfficiency(%)\n------\t\t----\t\t------\t\t-------\t\t------------\n";
    std::cout << table_header;
    out << table_header;
    
    // Sequential row
    std::cout << "0\t\tSequential\t" << sequential_time << "\t\tN/A\t\tN/A\n";
    out << "0\t\tSequential\t" << sequential_time << "\t\tN/A\t\tN/A\n";
    
    // Parallel rows
    for (const auto& res : results) {
        std::cout << res.cutoff << "\t\tParallel\t" << res.parallel_time << "\t\t" << res.speedup << "\t\t" << res.efficiency << "\n";
        out << res.cutoff << "\t\tParallel\t" << res.parallel_time << "\t\t" << res.speedup << "\t\t" << res.efficiency << "\n";
    }
    
    std::cout << "\nSequential run time (cutoff 0): " << sequential_time << " seconds\n";
    out << "\nSequential run time (cutoff 0): " << sequential_time << " seconds\n";

    return 0;
}