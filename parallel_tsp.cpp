#include <iostream>
#include <iomanip>
#include <chrono>
#include "modified_tsptask.hpp"
#include "parallel_task_runner.hpp"

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
    
    // Create task with cutoff 0 (split all the way)
    // Create task with chosen cutoff
    ModifiedTSPTask* tsp_task = new ModifiedTSPTask(cutoff);
    
    // Run parallel version
    std::cout << "\nRunning parallel version with " << num_threads << " threads..." << std::endl;
    
    ParallelTaskRunner parallel_runner(num_threads);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    parallel_runner.run(tsp_task);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    double parallel_time = std::chrono::duration<double>(end_time - start_time).count();
    
    
    TSPPath best_path = tsp_task->result();
    
    std::cout << "\n=== PARALLEL RESULTS ===" << std::endl;
    std::cout << "Best distance: " << best_path.distance() << std::endl;
    std::cout << "Time: " << std::fixed << std::setprecision(3) << parallel_time << " seconds" << std::endl;
    std::cout << "Tasks processed: " << parallel_runner.getTasksProcessed() << std::endl;
    std::cout << "Tasks created: " << parallel_runner.getTasksCreated() << std::endl;
    
    
    std::cout << "\nRunning sequential version for comparison..." << std::endl;
    
    ModifiedTSPTask seq_task(cutoff);
    DirectTaskRunner seq_runner;
    
    start_time = std::chrono::high_resolution_clock::now();
    seq_runner.run(&seq_task);
    end_time = std::chrono::high_resolution_clock::now();
    
    double seq_time = std::chrono::duration<double>(end_time - start_time).count();
    TSPPath seq_best = seq_task.result();
    
    std::cout << "\n=== SEQUENTIAL RESULTS ===" << std::endl;
    std::cout << "Best distance: " << seq_best.distance() << std::endl;
    std::cout << "Time: " << std::fixed << std::setprecision(3) << seq_time << " seconds" << std::endl;
    
    // Verify results match
    if (best_path.distance() == seq_best.distance()) {
        std::cout << "\n✓ Results match! Parallel solution is correct." << std::endl;
    } else {
        std::cout << "\n✗ ERROR: Results don't match!" << std::endl;
        std::cout << "Parallel: " << best_path.distance() << std::endl;
        std::cout << "Sequential: " << seq_best.distance() << std::endl;
    }
    
    // Calculate speedup
    if (seq_time > 0) {
        double speedup = seq_time / parallel_time;
        double efficiency = speedup / num_threads;
        
        std::cout << "\n=== PERFORMANCE ===" << std::endl;
        std::cout << "Speedup: " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
        std::cout << "Efficiency: " << std::fixed << std::setprecision(2) << (efficiency * 100) << "%" << std::endl;
    }
    
    return 0;
}