# NET-305: Automated Performance Regression Detection

| Field | Value |
|-------|-------|
| **ID** | NET-305 |
| **Title** | Automated Performance Regression Detection |
| **Category** | TEST |
| **Priority** | LOW |
| **Status** | DONE |
| **Est. Duration** | 4-5 days |
| **Dependencies** | NET-202 (Benchmark Reactivation) |
| **Target Version** | v1.9.0 |

---

## What (Problem Statement)

### Current Problem
Performance regressions can be introduced without detection:
- No automated performance testing in CI
- No baseline comparisons
- No alerts for performance degradation
- Manual performance testing is inconsistent

### Goal
Implement automated performance regression detection:
- Benchmark runs in CI pipeline
- Historical baseline comparisons
- Automatic alerts for significant regressions
- Performance tracking dashboard

### Benefits
- Early detection of performance issues
- Prevent shipping degraded performance
- Historical performance trends
- Data-driven optimization decisions

---

## How (Implementation Plan)

### Step 1: Define Performance Metrics

```cpp
// benchmarks/performance_metrics.h

struct performance_metrics {
    // Throughput metrics
    double requests_per_second;
    double messages_per_second;
    double bytes_per_second;

    // Latency metrics (microseconds)
    double latency_p50;
    double latency_p95;
    double latency_p99;
    double latency_p999;
    double latency_max;

    // Resource metrics
    size_t peak_memory_bytes;
    double cpu_usage_percent;

    // Metadata
    std::string commit_hash;
    std::string branch;
    std::chrono::system_clock::time_point timestamp;
};

struct regression_threshold {
    double throughput_decrease_percent = 10.0;  // Alert if > 10% decrease
    double latency_increase_percent = 20.0;     // Alert if > 20% increase
    double memory_increase_percent = 15.0;      // Alert if > 15% increase
};
```

### Step 2: Create Benchmark Runner

```python
#!/usr/bin/env python3
# scripts/run_regression_benchmarks.py

import json
import subprocess
import sys
from datetime import datetime
from pathlib import Path

def run_benchmarks(build_dir: str) -> dict:
    """Run benchmarks and collect results."""
    result = subprocess.run(
        [f"{build_dir}/benchmarks/performance_benchmark",
         "--benchmark_format=json",
         "--benchmark_out=benchmark_results.json"],
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        raise RuntimeError(f"Benchmark failed: {result.stderr}")

    with open("benchmark_results.json") as f:
        return json.load(f)

def extract_metrics(benchmark_results: dict) -> dict:
    """Extract key metrics from benchmark results."""
    metrics = {}

    for benchmark in benchmark_results.get("benchmarks", []):
        name = benchmark["name"]

        if "SimpleGet" in name:
            metrics["http_rps"] = benchmark.get("items_per_second", 0)
            metrics["http_latency_p50"] = benchmark.get("counters", {}).get("p50", 0)
            metrics["http_latency_p99"] = benchmark.get("counters", {}).get("p99", 0)

        elif "MessageThroughput" in name:
            metrics["msg_throughput"] = benchmark.get("items_per_second", 0)

        elif "ConnectionScaling" in name:
            metrics["max_connections"] = benchmark.get("counters", {}).get("connections", 0)

    metrics["timestamp"] = datetime.utcnow().isoformat()
    metrics["commit"] = get_git_commit()

    return metrics

def get_git_commit() -> str:
    """Get current git commit hash."""
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        capture_output=True,
        text=True
    )
    return result.stdout.strip()[:12]

def main():
    build_dir = sys.argv[1] if len(sys.argv) > 1 else "build"

    print("Running benchmarks...")
    results = run_benchmarks(build_dir)

    print("Extracting metrics...")
    metrics = extract_metrics(results)

    # Save results
    output_file = f"benchmark_metrics_{metrics['commit']}.json"
    with open(output_file, "w") as f:
        json.dump(metrics, f, indent=2)

    print(f"Results saved to {output_file}")
    print(json.dumps(metrics, indent=2))

if __name__ == "__main__":
    main()
```

### Step 3: Implement Regression Detection

```python
#!/usr/bin/env python3
# scripts/detect_regression.py

import json
import sys
from dataclasses import dataclass
from typing import Optional

@dataclass
class RegressionResult:
    metric: str
    baseline_value: float
    current_value: float
    change_percent: float
    threshold_percent: float
    is_regression: bool

def load_baseline(baseline_path: str) -> dict:
    """Load baseline metrics."""
    with open(baseline_path) as f:
        return json.load(f)

def load_current(current_path: str) -> dict:
    """Load current metrics."""
    with open(current_path) as f:
        return json.load(f)

def check_regression(
    baseline: float,
    current: float,
    threshold_percent: float,
    higher_is_better: bool = True
) -> RegressionResult:
    """Check if a metric has regressed."""
    if baseline == 0:
        change_percent = 0 if current == 0 else float('inf')
    else:
        change_percent = ((current - baseline) / baseline) * 100

    if higher_is_better:
        is_regression = change_percent < -threshold_percent
    else:
        is_regression = change_percent > threshold_percent

    return RegressionResult(
        metric="",
        baseline_value=baseline,
        current_value=current,
        change_percent=change_percent,
        threshold_percent=threshold_percent,
        is_regression=is_regression
    )

def detect_regressions(baseline: dict, current: dict) -> list[RegressionResult]:
    """Detect all regressions."""
    results = []

    # Throughput metrics (higher is better)
    throughput_metrics = [
        ("http_rps", 10.0),
        ("msg_throughput", 10.0),
    ]

    for metric, threshold in throughput_metrics:
        if metric in baseline and metric in current:
            result = check_regression(
                baseline[metric],
                current[metric],
                threshold,
                higher_is_better=True
            )
            result.metric = metric
            results.append(result)

    # Latency metrics (lower is better)
    latency_metrics = [
        ("http_latency_p50", 20.0),
        ("http_latency_p99", 30.0),
    ]

    for metric, threshold in latency_metrics:
        if metric in baseline and metric in current:
            result = check_regression(
                baseline[metric],
                current[metric],
                threshold,
                higher_is_better=False
            )
            result.metric = metric
            results.append(result)

    return results

def generate_report(results: list[RegressionResult]) -> str:
    """Generate regression report."""
    lines = ["# Performance Regression Report\n"]

    regressions = [r for r in results if r.is_regression]
    improvements = [r for r in results if r.change_percent > 5]  # > 5% improvement

    if regressions:
        lines.append("## :x: Regressions Detected\n")
        for r in regressions:
            lines.append(f"- **{r.metric}**: {r.baseline_value:.2f} -> {r.current_value:.2f} "
                        f"({r.change_percent:+.1f}%, threshold: {r.threshold_percent}%)")
        lines.append("")

    if improvements:
        lines.append("## :white_check_mark: Improvements\n")
        for r in improvements:
            lines.append(f"- **{r.metric}**: {r.baseline_value:.2f} -> {r.current_value:.2f} "
                        f"({r.change_percent:+.1f}%)")
        lines.append("")

    lines.append("## All Metrics\n")
    lines.append("| Metric | Baseline | Current | Change |")
    lines.append("|--------|----------|---------|--------|")
    for r in results:
        status = ":x:" if r.is_regression else ":white_check_mark:"
        lines.append(f"| {r.metric} | {r.baseline_value:.2f} | {r.current_value:.2f} | "
                    f"{r.change_percent:+.1f}% {status} |")

    return "\n".join(lines)

def main():
    if len(sys.argv) < 3:
        print("Usage: detect_regression.py <baseline.json> <current.json>")
        sys.exit(1)

    baseline = load_baseline(sys.argv[1])
    current = load_current(sys.argv[2])

    results = detect_regressions(baseline, current)
    report = generate_report(results)

    print(report)

    # Exit with error if regressions detected
    if any(r.is_regression for r in results):
        print("\n:warning: Performance regressions detected!")
        sys.exit(1)

if __name__ == "__main__":
    main()
```

### Step 4: CI Integration

```yaml
# .github/workflows/performance.yml

name: Performance Regression Check

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake ninja-build

      - name: Build Release
        run: |
          cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_BENCHMARKS=ON
          cmake --build build --target performance_benchmark

      - name: Download baseline
        uses: actions/download-artifact@v4
        with:
          name: performance-baseline
          path: baseline/
        continue-on-error: true

      - name: Run benchmarks
        run: python3 scripts/run_regression_benchmarks.py build

      - name: Check for regressions
        if: hashFiles('baseline/baseline.json') != ''
        run: |
          python3 scripts/detect_regression.py \
            baseline/baseline.json \
            benchmark_metrics_*.json > regression_report.md

      - name: Comment on PR
        if: github.event_name == 'pull_request'
        uses: actions/github-script@v7
        with:
          script: |
            const fs = require('fs');
            const report = fs.readFileSync('regression_report.md', 'utf8');
            github.rest.issues.createComment({
              owner: context.repo.owner,
              repo: context.repo.repo,
              issue_number: context.issue.number,
              body: report
            });

      - name: Update baseline (main only)
        if: github.ref == 'refs/heads/main'
        run: |
          mkdir -p baseline
          cp benchmark_metrics_*.json baseline/baseline.json

      - name: Upload new baseline
        if: github.ref == 'refs/heads/main'
        uses: actions/upload-artifact@v4
        with:
          name: performance-baseline
          path: baseline/
```

### Step 5: Dashboard Integration (Optional)

```python
#!/usr/bin/env python3
# scripts/upload_metrics.py

import json
import os
import requests
from datetime import datetime

def upload_to_grafana_cloud(metrics: dict):
    """Upload metrics to Grafana Cloud for visualization."""
    api_key = os.environ.get("GRAFANA_API_KEY")
    if not api_key:
        print("GRAFANA_API_KEY not set, skipping upload")
        return

    endpoint = "https://influx-prod-us-central1.grafana.net/api/v1/push/influx/write"

    lines = []
    timestamp_ns = int(datetime.utcnow().timestamp() * 1e9)

    for metric, value in metrics.items():
        if isinstance(value, (int, float)):
            lines.append(f"network_system,commit={metrics['commit']} {metric}={value} {timestamp_ns}")

    response = requests.post(
        endpoint,
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "text/plain"
        },
        data="\n".join(lines)
    )

    if response.status_code != 204:
        print(f"Failed to upload metrics: {response.text}")
    else:
        print("Metrics uploaded successfully")
```

---

## Test (Verification Plan)

### Local Testing
```bash
# Run benchmarks locally
python3 scripts/run_regression_benchmarks.py build

# Create fake baseline
cp benchmark_metrics_*.json baseline.json

# Modify code to introduce regression
# ... make performance worse ...

# Run regression check
python3 scripts/detect_regression.py baseline.json benchmark_metrics_*.json
```

### CI Testing
1. Create PR with no changes - should pass
2. Create PR with performance regression - should fail
3. Verify PR comment with report
4. Verify baseline updates on main

---

## Acceptance Criteria

- [ ] Benchmark runner script working
- [ ] Regression detection logic correct
- [ ] CI workflow integrated
- [ ] PR comments with reports
- [ ] Baseline management automated
- [ ] Thresholds configurable
- [ ] False positive rate acceptable
- [ ] Documentation complete

---

## Notes

- Benchmarks should run on consistent hardware for accurate comparisons
- Consider using GitHub-hosted larger runners for consistency
- May need warmup runs before measurement
- Statistical significance testing could reduce false positives
- Keep baseline history for trend analysis
