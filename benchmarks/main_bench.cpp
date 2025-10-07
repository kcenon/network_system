/**
 * @file main_bench.cpp
 * @brief Main entry point for network_system benchmarks
 * Phase 0, Task 0.2
 */

#include <benchmark/benchmark.h>
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "========================================\n";
    std::cout << "network_system Performance Benchmarks\n";
    std::cout << "Phase 0: Baseline Measurement\n";
    std::cout << "========================================\n\n";

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    std::cout << "\nBenchmarks Complete\n";
    return 0;
}
