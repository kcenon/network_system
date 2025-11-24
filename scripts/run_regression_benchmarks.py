#!/usr/bin/env python3
"""
BSD 3-Clause License
Copyright (c) 2024-2025, Network System Project

Performance Regression Benchmark Runner

This script runs benchmarks and exports results in JSON format for
regression detection.

Usage:
    run_regression_benchmarks.py [build_dir] [output_file]

Examples:
    python3 scripts/run_regression_benchmarks.py
    python3 scripts/run_regression_benchmarks.py build benchmark_results.json
"""

import json
import subprocess
import sys
import os
from datetime import datetime
from pathlib import Path
from typing import Dict, Optional


def get_git_commit() -> str:
    """Get current git commit hash."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()[:12]
    except subprocess.CalledProcessError:
        return "unknown"


def get_git_branch() -> str:
    """Get current git branch name."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return "unknown"


def find_benchmark_executable(build_dir: str) -> Optional[Path]:
    """Find the benchmark executable in the build directory."""
    possible_paths = [
        Path(build_dir) / "bin" / "benchmark",
        Path(build_dir) / "benchmarks" / "benchmark",
        Path(build_dir) / "bin" / "network_benchmarks",
        Path(build_dir) / "benchmarks" / "network_benchmarks",
    ]

    for path in possible_paths:
        if path.exists() and path.is_file():
            return path

    return None


def run_benchmarks(build_dir: str) -> Dict:
    """Run benchmarks and collect results."""
    benchmark_exe = find_benchmark_executable(build_dir)

    if not benchmark_exe:
        print(f"ERROR: Benchmark executable not found in {build_dir}", file=sys.stderr)
        print("Try building with: cmake --build build --target network_benchmarks", file=sys.stderr)
        sys.exit(1)

    print(f"Running benchmarks: {benchmark_exe}")

    # Run benchmark with JSON output
    json_output = Path(build_dir) / "benchmark_output.json"

    try:
        result = subprocess.run(
            [str(benchmark_exe), f"--benchmark_format=json", f"--benchmark_out={json_output}"],
            capture_output=True,
            text=True,
            timeout=300  # 5 minute timeout
        )

        if result.returncode != 0:
            print(f"WARNING: Benchmark exited with code {result.returncode}", file=sys.stderr)
            print(f"Stdout: {result.stdout}", file=sys.stderr)
            print(f"Stderr: {result.stderr}", file=sys.stderr)

        # If JSON output exists, use it
        if json_output.exists():
            with open(json_output, 'r') as f:
                return json.load(f)

        # Fallback: parse text output
        return parse_text_output(result.stdout)

    except subprocess.TimeoutExpired:
        print("ERROR: Benchmark timed out after 5 minutes", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: Failed to run benchmark: {e}", file=sys.stderr)
        sys.exit(1)


def parse_text_output(output: str) -> Dict:
    """Parse benchmark text output (fallback)."""
    # This is a simple parser for text output
    # Actual implementation depends on benchmark output format
    return {
        "context": {
            "date": datetime.utcnow().isoformat(),
            "num_cpus": os.cpu_count(),
            "mhz_per_cpu": 0,
            "cpu_scaling_enabled": False,
            "library_build_type": "release"
        },
        "benchmarks": []
    }


def extract_metrics(benchmark_results: Dict) -> Dict:
    """Extract key performance metrics from benchmark results."""
    metrics = {
        "timestamp": datetime.utcnow().isoformat(),
        "commit": get_git_commit(),
        "branch": get_git_branch()
    }

    benchmarks = benchmark_results.get("benchmarks", [])

    for benchmark in benchmarks:
        name = benchmark.get("name", "")

        # Extract throughput metrics
        if "items_per_second" in benchmark:
            metrics[f"{name}_throughput"] = benchmark["items_per_second"]

        # Extract time metrics
        if "real_time" in benchmark:
            metrics[f"{name}_time_ns"] = benchmark["real_time"]

        # Extract custom counters (latency percentiles, etc.)
        if "counters" in benchmark:
            for counter_name, counter_value in benchmark["counters"].items():
                metrics[f"{name}_{counter_name}"] = counter_value

    return metrics


def save_metrics(metrics: Dict, output_file: str) -> None:
    """Save metrics to JSON file."""
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'w') as f:
        json.dump(metrics, f, indent=2, sort_keys=True)

    print(f"Metrics saved to {output_file}")


def print_summary(metrics: Dict) -> None:
    """Print a summary of collected metrics."""
    print("\n" + "="*60)
    print("BENCHMARK RESULTS SUMMARY")
    print("="*60)
    print(f"Commit: {metrics.get('commit', 'unknown')}")
    print(f"Branch: {metrics.get('branch', 'unknown')}")
    print(f"Timestamp: {metrics.get('timestamp', 'unknown')}")
    print()

    # Print throughput metrics
    throughput_metrics = {k: v for k, v in metrics.items() if 'throughput' in k}
    if throughput_metrics:
        print("Throughput:")
        for key, value in sorted(throughput_metrics.items()):
            print(f"  {key}: {value:,.2f} ops/sec")
        print()

    # Print latency metrics
    latency_metrics = {k: v for k, v in metrics.items() if any(p in k for p in ['p50', 'p95', 'p99', 'latency'])}
    if latency_metrics:
        print("Latency:")
        for key, value in sorted(latency_metrics.items()):
            print(f"  {key}: {value:.2f} us")
        print()

    print("="*60)


def main():
    build_dir = sys.argv[1] if len(sys.argv) > 1 else "build"
    output_file = sys.argv[2] if len(sys.argv) > 2 else f"benchmark_metrics_{get_git_commit()}.json"

    print(f"Build directory: {build_dir}")
    print(f"Output file: {output_file}")
    print()

    # Check if build directory exists
    if not Path(build_dir).exists():
        print(f"ERROR: Build directory not found: {build_dir}", file=sys.stderr)
        print("Please build the project first:", file=sys.stderr)
        print(f"  cmake -B {build_dir} -DNETWORK_BUILD_BENCHMARKS=ON", file=sys.stderr)
        print(f"  cmake --build {build_dir} --target network_benchmarks", file=sys.stderr)
        sys.exit(1)

    # Run benchmarks
    print("Running benchmarks...")
    results = run_benchmarks(build_dir)

    # Extract metrics
    print("Extracting metrics...")
    metrics = extract_metrics(results)

    # Save results
    save_metrics(metrics, output_file)

    # Print summary
    print_summary(metrics)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrupted by user", file=sys.stderr)
        sys.exit(130)
    except Exception as e:
        print(f"FATAL ERROR: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)
