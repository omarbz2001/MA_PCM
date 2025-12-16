import subprocess
import sys
import os
import re
import matplotlib.pyplot as plt

def run_parallel_tsp(tsp_file, num_cities, threads):
    """
    Runs the parallel_tsp executable and extracts the parallel execution time.
    """
    cmd = ["./parallel_tsp", tsp_file, str(num_cities), str(threads)]
    
    print(f"\nRunning with {threads} threads...")
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    output = result.stdout

    #Extract time 
    match = re.search(r"Time:\s+([0-9]*\.?[0-9]+)\s+seconds", output)
    
    if not match:
        print("ERROR: Could not extract time from output")
        print(output)
        sys.exit(1)

    time_sec = float(match.group(1))
    print(f"Extracted time: {time_sec} seconds")

    return time_sec


def main():
    if len(sys.argv) < 5:
        print("Usage:")
        print("  python speedup_plot.py <file.tsp> <num_cities> <N> <t1> <t2> ... <tN>")
        print("Example:")
        print("  python speedup_plot.py dj38.tsp 20 5 2 4 6 8 12")
        sys.exit(1)

    tsp_file = sys.argv[1]
    num_cities = int(sys.argv[2])
    N = int(sys.argv[3])
    thread_counts = list(map(int, sys.argv[4:]))

    if len(thread_counts) != N:
        print("ERROR: Number of thread values does not match N")
        sys.exit(1)

    
    os.makedirs("plots", exist_ok=True)

    times = []

    for threads in thread_counts:
        time_sec = run_parallel_tsp(tsp_file, num_cities, threads)
        times.append(time_sec)

    
    plt.figure()
    plt.plot(thread_counts, times, marker='o')
    plt.xlabel("Number of Threads")
    plt.ylabel("Execution Time (seconds)")
    plt.title("Parallel TSP Execution Time vs Threads")
    plt.grid(True)

    output_file = f"plots/parallel_time_{os.path.splitext(tsp_file)[0]}_{num_cities}.png"
    plt.savefig(output_file)
    plt.close()

    print("\n=== DONE ===")
    print("Threads:", thread_counts)
    print("Times:", times)
    print(f"Plot saved to: {output_file}")


if __name__ == "__main__":
    main()
