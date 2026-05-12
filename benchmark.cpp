#include "benchmark.hpp"

#include "solver.hpp"

#include <array>
#include <iomanip>
#include <iostream>
#include <string>

void runBenchmark(const Board& puzzle) {
    struct BenchmarkCase {
        std::string name;
        SolverOptions options;
    };

    SolverOptions normalDfs;
    normalDfs.useMrv = false;
    normalDfs.usePropagation = false;
    normalDfs.useOptimizedIteration = false;

    SolverOptions mrvOnly;
    mrvOnly.usePropagation = false;

    SolverOptions mrvWithPropagation;

    SolverOptions uniquenessCheck;
    uniquenessCheck.solutionLimit = 2;

    std::array<BenchmarkCase, 4> cases{{
        {"Normal DFS, simple digit loop", normalDfs},
        {"MRV only", mrvOnly},
        {"MRV + propagation", mrvWithPropagation},
        {"Uniqueness check", uniquenessCheck},
    }};

    std::cout << "\nBenchmark:\n";
    std::cout << std::left << std::setw(30) << "Mode"
              << std::right << std::setw(12) << "Calls"
              << std::setw(12) << "Backtracks"
              << std::setw(12) << "Depth"
              << std::setw(12) << "Time(ms)"
              << "  Result\n";

    for (const auto& benchmarkCase : cases) {
        SolveReport report = solveSudoku(puzzle, benchmarkCase.options);
        std::cout << std::left << std::setw(30) << benchmarkCase.name
                  << std::right << std::setw(12) << report.stats.recursiveCalls
                  << std::setw(12) << report.stats.backtracks
                  << std::setw(12) << report.stats.maxDepth
                  << std::setw(12) << std::fixed << std::setprecision(3) << report.stats.elapsedMs
                  << "  " << statusName(report.status) << '\n';
    }
}
