#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <fstream>
#include <string>
#include "tsptask.hpp"
#include "parallel_task_runner.hpp"

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
        cutoffs = {0, 2, 4, 6}; // Default cutoffs to test
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
    
    TSPPath::setup(&graph);
    
    // Compare each cutoff
    std::vector<std::pair<int, double>> results;
    
    for (int cutoff : cutoffs) {
        std::cout << "Running parallel TSP with cutoff " << cutoff << "...\n";
        TSPTask* tsp_task = new TSPTask(cutoff);
        ParallelTaskRunner parallel_runner(num_threads);
        parallel_runner.run(tsp_task);
        
        results.emplace_back(cutoff, parallel_runner.duration());
    }

    
    // Create results directory and file
    std::string base_name = filename.substr(0, filename.find_last_of('.'));
    std::string file_path = "results/cutoff/" + base_name + "_" + std::to_string(graph.size()) + "_" + std::to_string(num_threads) + ".txt";
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
    
    std::string table_header = "Cutoff\t\tParallel(s)\n------\t\t-----------\n";
    std::cout << table_header;
    out << table_header;
    
    for (const auto& res : results) {
        std::cout << res.first << "\t\t" << res.second << "\n";
        out << res.first << "\t\t" << res.second << "\n";
    }
    
    return 0;
}
