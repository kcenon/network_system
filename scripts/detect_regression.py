#!/usr/bin/env python3
"""
BSD 3-Clause License
Copyright (c) 2024-2025, Network System Project

Performance Regression Detection

This script compares current benchmark results against a baseline and
detects performance regressions.

Usage:
    detect_regression.py <baseline.json> <current.json> [--report=<file>]

Examples:
    python3 scripts/detect_regression.py baseline.json current.json
    python3 scripts/detect_regression.py baseline.json current.json --report=report.md
"""

import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional


@dataclass
class RegressionResult:
    """Result of a regression check for a single metric."""
    metric: str
    baseline_value: float
    current_value: float
    change_percent: float
    threshold_percent: float
    is_regression: bool

    def __str__(self):
        direction = "▼" if self.is_regression else "▲"
        sign = "+" if self.change_percent > 0 else ""
        return (f"{direction} {self.metric}: {self.baseline_value:.2f} → {self.current_value:.2f} "
                f"({sign}{self.change_percent:.1f}%)")


# Default regression thresholds
DEFAULT_THRESHOLDS = {
    # Throughput metrics (higher is better, alert if > 10% decrease)
    "throughput": {"threshold": 10.0, "higher_is_better": True},

    # Time metrics (lower is better, alert if > 20% increase)
    "time_ns": {"threshold": 20.0, "higher_is_better": False},
    "time_us": {"threshold": 20.0, "higher_is_better": False},
    "time_ms": {"threshold": 20.0, "higher_is_better": False},

    # Latency percentiles (lower is better)
    "p50": {"threshold": 20.0, "higher_is_better": False},
    "p95": {"threshold": 30.0, "higher_is_better": False},
    "p99": {"threshold": 30.0, "higher_is_better": False},
    "p999": {"threshold": 40.0, "higher_is_better": False},

    # Memory metrics (lower is better)
    "memory": {"threshold": 15.0, "higher_is_better": False},
    "allocs": {"threshold": 20.0, "higher_is_better": False},
}


def load_metrics(path: str) -> Dict:
    """Load metrics from JSON file."""
    try:
        with open(path, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"ERROR: File not found: {path}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"ERROR: Invalid JSON in {path}: {e}", file=sys.stderr)
        sys.exit(1)


def get_threshold_config(metric_name: str) -> Dict:
    """Get threshold configuration for a metric."""
    # Check for exact match first
    for key, config in DEFAULT_THRESHOLDS.items():
        if key in metric_name.lower():
            return config

    # Default: throughput-like (higher is better)
    return {"threshold": 10.0, "higher_is_better": True}


def check_regression(
    metric_name: str,
    baseline: float,
    current: float
) -> RegressionResult:
    """Check if a metric has regressed."""
    config = get_threshold_config(metric_name)
    threshold = config["threshold"]
    higher_is_better = config["higher_is_better"]

    if baseline == 0:
        change_percent = 0 if current == 0 else float('inf')
    else:
        change_percent = ((current - baseline) / baseline) * 100

    # Determine if this is a regression
    if higher_is_better:
        # For metrics where higher is better (e.g., throughput)
        # Regression = significant decrease
        is_regression = change_percent < -threshold
    else:
        # For metrics where lower is better (e.g., latency, memory)
        # Regression = significant increase
        is_regression = change_percent > threshold

    return RegressionResult(
        metric=metric_name,
        baseline_value=baseline,
        current_value=current,
        change_percent=change_percent,
        threshold_percent=threshold,
        is_regression=is_regression
    )


def detect_regressions(baseline: Dict, current: Dict) -> List[RegressionResult]:
    """Detect all regressions by comparing current metrics to baseline."""
    results = []

    # Get all numeric metrics (exclude metadata)
    metadata_keys = {'timestamp', 'commit', 'branch'}

    # Compare common metrics
    for key in baseline.keys():
        if key in metadata_keys:
            continue

        if key not in current:
            continue  # Metric removed or renamed

        baseline_value = baseline[key]
        current_value = current[key]

        # Skip non-numeric values
        if not isinstance(baseline_value, (int, float)) or not isinstance(current_value, (int, float)):
            continue

        result = check_regression(key, baseline_value, current_value)
        results.append(result)

    return results


def generate_console_report(results: List[RegressionResult], baseline: Dict, current: Dict) -> int:
    """Generate console report and return exit code."""
    regressions = [r for r in results if r.is_regression]
    improvements = [r for r in results if r.change_percent > 5 and not r.is_regression]

    print()
    print("="*70)
    print("PERFORMANCE REGRESSION REPORT")
    print("="*70)
    print(f"Baseline: {baseline.get('commit', 'unknown')} ({baseline.get('branch', 'unknown')})")
    print(f"Current:  {current.get('commit', 'unknown')} ({current.get('branch', 'unknown')})")
    print()

    if regressions:
        print(f"⚠️  REGRESSIONS DETECTED ({len(regressions)})")
        print("-"*70)
        for r in regressions:
            print(f"  {r}")
        print()

    if improvements:
        print(f"✅ IMPROVEMENTS ({len(improvements)})")
        print("-"*70)
        for r in improvements:
            print(f"  {r}")
        print()

    if not regressions and not improvements:
        print("✅ No significant changes detected")
        print()

    print(f"Total metrics compared: {len(results)}")
    print("="*70)

    return 1 if regressions else 0


def generate_markdown_report(results: List[RegressionResult], baseline: Dict, current: Dict) -> str:
    """Generate markdown report."""
    lines = ["# Performance Regression Report\n"]

    lines.append("## Metadata\n")
    lines.append(f"- **Baseline**: `{baseline.get('commit', 'unknown')}` ({baseline.get('branch', 'unknown')})")
    lines.append(f"- **Current**: `{current.get('commit', 'unknown')}` ({current.get('branch', 'unknown')})")
    lines.append(f"- **Baseline Date**: {baseline.get('timestamp', 'unknown')}")
    lines.append(f"- **Current Date**: {current.get('timestamp', 'unknown')}\n")

    regressions = [r for r in results if r.is_regression]
    improvements = [r for r in results if r.change_percent > 5 and not r.is_regression]

    if regressions:
        lines.append("## ⚠️ Regressions Detected\n")
        lines.append("| Metric | Baseline | Current | Change | Threshold |")
        lines.append("|--------|----------|---------|--------|-----------|")
        for r in regressions:
            sign = "+" if r.change_percent > 0 else ""
            lines.append(f"| {r.metric} | {r.baseline_value:.2f} | {r.current_value:.2f} | "
                        f"{sign}{r.change_percent:.1f}% | {r.threshold_percent:.0f}% |")
        lines.append("")

    if improvements:
        lines.append("## ✅ Improvements\n")
        lines.append("| Metric | Baseline | Current | Change |")
        lines.append("|--------|----------|---------|--------|")
        for r in improvements:
            sign = "+" if r.change_percent > 0 else ""
            lines.append(f"| {r.metric} | {r.baseline_value:.2f} | {r.current_value:.2f} | "
                        f"{sign}{r.change_percent:.1f}% |")
        lines.append("")

    if not regressions and not improvements:
        lines.append("## ✅ No Significant Changes\n")
        lines.append("All metrics are within acceptable thresholds.\n")

    lines.append("## All Metrics\n")
    lines.append("| Metric | Baseline | Current | Change | Status |")
    lines.append("|--------|----------|---------|--------|--------|")
    for r in sorted(results, key=lambda x: abs(x.change_percent), reverse=True):
        sign = "+" if r.change_percent > 0 else ""
        status = "⚠️ Regression" if r.is_regression else ("✅ Improved" if r.change_percent > 5 else "➡️ Unchanged")
        lines.append(f"| {r.metric} | {r.baseline_value:.2f} | {r.current_value:.2f} | "
                    f"{sign}{r.change_percent:.1f}% | {status} |")

    lines.append("")
    lines.append(f"**Total Metrics**: {len(results)}")
    lines.append(f"**Regressions**: {len(regressions)}")
    lines.append(f"**Improvements**: {len(improvements)}")

    return "\n".join(lines)


def save_report(report: str, path: str) -> None:
    """Save report to file."""
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'w') as f:
        f.write(report)

    print(f"\nReport saved to {path}")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    baseline_path = sys.argv[1]
    current_path = sys.argv[2]

    # Parse optional report file argument
    report_path = None
    for arg in sys.argv[3:]:
        if arg.startswith("--report="):
            report_path = arg.split("=", 1)[1]

    # Load metrics
    baseline = load_metrics(baseline_path)
    current = load_metrics(current_path)

    # Detect regressions
    results = detect_regressions(baseline, current)

    # Generate console report
    exit_code = generate_console_report(results, baseline, current)

    # Generate markdown report if requested
    if report_path:
        report = generate_markdown_report(results, baseline, current)
        save_report(report, report_path)

    return exit_code


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
