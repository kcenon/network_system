#!/usr/bin/env python3
"""
Update BASELINE.md with new performance metrics from load tests.

This script reads aggregated metrics and updates the baseline documentation
with real network performance data.
"""

import argparse
import json
import sys
import re
from pathlib import Path
from typing import Dict, Any, List, Optional
from datetime import datetime


def load_metrics(metrics_path: str) -> Optional[Dict[str, Any]]:
    """Load metrics from JSON file."""
    try:
        with open(metrics_path, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: Metrics file not found: {metrics_path}", file=sys.stderr)
        return None
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in metrics file: {e}", file=sys.stderr)
        return None


def load_baseline(baseline_path: str) -> Optional[str]:
    """Load existing baseline file."""
    try:
        with open(baseline_path, 'r') as f:
            return f.read()
    except FileNotFoundError:
        print(f"Error: Baseline file not found: {baseline_path}", file=sys.stderr)
        return None


def format_metric_table_row(protocol: str, data: Dict[str, Any], platform: str) -> str:
    """Format a single row for the metrics table."""
    return (
        f"| {protocol.upper():10s} | "
        f"{data['throughput_msg_s']:>12.0f} | "
        f"{data['latency']['p50_ms']:>12.2f} | "
        f"{data['latency']['p95_ms']:>12.2f} | "
        f"{data['latency']['p99_ms']:>12.2f} | "
        f"{data['memory']['rss_mb']:>12.2f} | "
        f"{platform:12s} |"
    )


def generate_network_metrics_section(metrics: Dict[str, Any], platform: str) -> str:
    """Generate the network performance metrics section."""
    timestamp = metrics.get('timestamp', datetime.utcnow().isoformat() + 'Z')
    protocols = metrics.get('protocols', {})

    lines = [
        "## Real Network Performance Metrics",
        "",
        f"**Last Updated:** {timestamp}",
        f"**Platform:** {platform}",
        "",
        "These metrics are collected from actual network load tests measuring real TCP/UDP/WebSocket",
        "communication over loopback. Unlike synthetic benchmarks, these reflect true end-to-end",
        "performance including network stack overhead, protocol framing, and I/O operations.",
        "",
        "### Load Test Results (64-byte messages, 1000 iterations)",
        "",
        "| Protocol   | Throughput   | Latency P50  | Latency P95  | Latency P99  | Memory (RSS) | Platform     |",
        "|------------|--------------|--------------|--------------|--------------|--------------|--------------|",
    ]

    # Add rows for each protocol
    for protocol in ['tcp', 'udp', 'websocket']:
        if protocol in protocols:
            lines.append(format_metric_table_row(protocol, protocols[protocol], platform))

    lines.extend([
        "",
        "### Measurement Methodology",
        "",
        "- **Throughput:** Messages per second (higher is better)",
        "- **Latency:** Time from send call to completion (lower is better)",
        "  - P50: 50th percentile (median)",
        "  - P95: 95th percentile",
        "  - P99: 99th percentile",
        "- **Memory:** Resident Set Size during test execution",
        "- **Test Configuration:**",
        "  - Message size: 64 bytes",
        "  - Iterations: 1000 messages",
        "  - Connection: Local loopback (localhost)",
        "  - Concurrency: Single client",
        "",
    ])

    return "\n".join(lines)


def update_baseline_content(
    baseline_content: str,
    metrics: Dict[str, Any],
    platform: str
) -> str:
    """Update baseline content with new metrics."""
    # Generate new metrics section
    new_section = generate_network_metrics_section(metrics, platform)

    # Pattern to match existing network metrics section
    pattern = r'## Real Network Performance Metrics.*?(?=\n##|\Z)'

    # Check if section exists
    if re.search(pattern, baseline_content, re.DOTALL):
        # Replace existing section
        updated_content = re.sub(
            pattern,
            new_section,
            baseline_content,
            flags=re.DOTALL
        )
    else:
        # Insert new section before "Baseline Tracking" or at the end
        insertion_pattern = r'(## Baseline Tracking)'
        if re.search(insertion_pattern, baseline_content):
            updated_content = re.sub(
                insertion_pattern,
                new_section + "\n\n\\1",
                baseline_content
            )
        else:
            # Append at the end
            updated_content = baseline_content.rstrip() + "\n\n" + new_section + "\n"

    # Update timestamp at the top of the file
    timestamp = datetime.utcnow().strftime("%Y-%m-%d")
    updated_content = re.sub(
        r'Last Updated: \d{4}-\d{2}-\d{2}',
        f'Last Updated: {timestamp}',
        updated_content
    )

    return updated_content


def write_baseline(baseline_path: str, content: str) -> bool:
    """Write updated baseline content to file."""
    try:
        with open(baseline_path, 'w') as f:
            f.write(content)
        return True
    except IOError as e:
        print(f"Error: Failed to write baseline file: {e}", file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description='Update BASELINE.md with new performance metrics'
    )
    parser.add_argument(
        '--metrics',
        type=str,
        required=True,
        help='Path to aggregated metrics JSON file'
    )
    parser.add_argument(
        '--baseline',
        type=str,
        required=True,
        help='Path to BASELINE.md file'
    )
    parser.add_argument(
        '--platform',
        type=str,
        required=True,
        help='Platform identifier (e.g., ubuntu-22.04, macos-13, windows-2022)'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show changes without writing to file'
    )

    args = parser.parse_args()

    # Load metrics
    metrics = load_metrics(args.metrics)
    if not metrics:
        return 1

    # Load baseline
    baseline_content = load_baseline(args.baseline)
    if not baseline_content:
        return 1

    # Update baseline content
    updated_content = update_baseline_content(baseline_content, metrics, args.platform)

    # Dry run mode - just print diff
    if args.dry_run:
        print("=" * 60)
        print("DRY RUN - Changes that would be made:")
        print("=" * 60)
        print(updated_content)
        print("=" * 60)
        return 0

    # Write updated baseline
    if not write_baseline(args.baseline, updated_content):
        return 1

    print(f"Successfully updated {args.baseline}")
    print(f"Platform: {args.platform}")
    print(f"Metrics: {len(metrics.get('protocols', {}))} protocols")

    return 0


if __name__ == '__main__':
    sys.exit(main())
