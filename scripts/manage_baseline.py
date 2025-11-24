#!/usr/bin/env python3
"""
BSD 3-Clause License
Copyright (c) 2024, Network System Project

Static Analysis Baseline Management Script

Usage:
    manage_baseline.py check <output_file> <baseline_file>
    manage_baseline.py update <output_file> <baseline_file>
    manage_baseline.py summary <baseline_file>
"""

import json
import sys
import re
from pathlib import Path
from typing import Dict, List, Set, Tuple


def load_baseline(path: str) -> Dict:
    """Load baseline from JSON file."""
    if not Path(path).exists():
        return {"warnings": [], "version": 1, "metadata": {}}
    with open(path, 'r') as f:
        return json.load(f)


def save_baseline(data: Dict, path: str) -> None:
    """Save baseline to JSON file."""
    with open(path, 'w') as f:
        json.dump(data, f, indent=2, sort_keys=True)


def parse_clang_tidy_output(output_path: str) -> List[Dict]:
    """Parse clang-tidy output and extract warnings."""
    warnings = []

    # Pattern: file:line:col: warning/error: message [check-name]
    pattern = re.compile(
        r'^(.+?):(\d+):(\d+):\s+(warning|error):\s+(.+?)\s*\[(.+?)\]',
        re.MULTILINE
    )

    with open(output_path, 'r') as f:
        content = f.read()

    for match in pattern.finditer(content):
        file_path, line, col, severity, message, check_name = match.groups()
        warnings.append({
            "file": file_path,
            "line": int(line),
            "column": int(col),
            "severity": severity,
            "message": message.strip(),
            "check": check_name
        })

    return warnings


def parse_cppcheck_xml(xml_path: str) -> List[Dict]:
    """Parse cppcheck XML output and extract issues."""
    import xml.etree.ElementTree as ET

    warnings = []

    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()

        for error in root.iter('error'):
            location = error.find('location')
            if location is not None:
                warnings.append({
                    "file": location.get('file', ''),
                    "line": int(location.get('line', 0)),
                    "column": int(location.get('column', 0)),
                    "severity": error.get('severity', ''),
                    "message": error.get('msg', ''),
                    "check": error.get('id', '')
                })
    except Exception as e:
        print(f"Error parsing XML: {e}", file=sys.stderr)

    return warnings


def get_warning_key(warning: Dict) -> Tuple[str, str, str]:
    """Get a unique key for a warning (excluding line numbers)."""
    # Normalize file path
    file_path = Path(warning["file"]).name
    return (file_path, warning["check"], warning["message"])


def compare_with_baseline(
    current: List[Dict],
    baseline: Dict
) -> Tuple[Set, Set]:
    """Compare current warnings with baseline."""
    baseline_set = {get_warning_key(w) for w in baseline.get("warnings", [])}
    current_set = {get_warning_key(w) for w in current}

    new_warnings = current_set - baseline_set
    fixed_warnings = baseline_set - current_set

    return new_warnings, fixed_warnings


def print_warnings_by_category(warnings: List[Dict]) -> None:
    """Print warnings grouped by category."""
    categories = {}
    for w in warnings:
        check = w.get("check", "unknown")
        if check not in categories:
            categories[check] = 0
        categories[check] += 1

    for check, count in sorted(categories.items(), key=lambda x: -x[1]):
        print(f"  {count:4d}  {check}")


def cmd_check(output_path: str, baseline_path: str) -> int:
    """Check current output against baseline."""
    print("Checking against baseline...")
    print(f"  Output: {output_path}")
    print(f"  Baseline: {baseline_path}")
    print()

    # Detect file type and parse
    if output_path.endswith('.xml'):
        current = parse_cppcheck_xml(output_path)
    else:
        current = parse_clang_tidy_output(output_path)

    baseline = load_baseline(baseline_path)

    new_warnings, fixed = compare_with_baseline(current, baseline)

    print(f"Current warnings: {len(current)}")
    print(f"Baseline warnings: {len(baseline.get('warnings', []))}")
    print()

    if fixed:
        print(f"FIXED: {len(fixed)} warning(s) resolved!")
        for w in list(fixed)[:10]:  # Show first 10
            print(f"  - {w[0]}: [{w[1]}] {w[2][:60]}...")
        if len(fixed) > 10:
            print(f"  ... and {len(fixed) - 10} more")
        print()

    if new_warnings:
        print(f"ERROR: {len(new_warnings)} NEW warning(s) introduced!")
        for w in list(new_warnings)[:10]:  # Show first 10
            print(f"  - {w[0]}: [{w[1]}] {w[2][:60]}...")
        if len(new_warnings) > 10:
            print(f"  ... and {len(new_warnings) - 10} more")
        print()
        print("Please fix these warnings before committing.")
        return 1

    print("OK: No new warnings introduced")

    if fixed:
        print()
        print("TIP: Run 'manage_baseline.py update' to update the baseline")

    return 0


def cmd_update(output_path: str, baseline_path: str) -> int:
    """Update baseline with current warnings."""
    print("Updating baseline...")
    print(f"  Output: {output_path}")
    print(f"  Baseline: {baseline_path}")
    print()

    # Detect file type and parse
    if output_path.endswith('.xml'):
        current = parse_cppcheck_xml(output_path)
    else:
        current = parse_clang_tidy_output(output_path)

    baseline = {
        "version": 1,
        "warnings": current,
        "metadata": {
            "source_file": output_path,
            "warning_count": len(current)
        }
    }

    save_baseline(baseline, baseline_path)

    print(f"Baseline updated with {len(current)} warnings")
    print()
    print("Warnings by category:")
    print_warnings_by_category(current)

    return 0


def cmd_summary(baseline_path: str) -> int:
    """Show summary of baseline."""
    print(f"Baseline summary: {baseline_path}")
    print()

    baseline = load_baseline(baseline_path)
    warnings = baseline.get("warnings", [])

    print(f"Total warnings: {len(warnings)}")
    print()
    print("Warnings by category:")
    print_warnings_by_category(warnings)

    return 0


def print_usage():
    """Print usage information."""
    print(__doc__)


def main():
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)

    command = sys.argv[1]

    if command == "check":
        if len(sys.argv) != 4:
            print("Usage: manage_baseline.py check <output_file> <baseline_file>")
            sys.exit(1)
        sys.exit(cmd_check(sys.argv[2], sys.argv[3]))

    elif command == "update":
        if len(sys.argv) != 4:
            print("Usage: manage_baseline.py update <output_file> <baseline_file>")
            sys.exit(1)
        sys.exit(cmd_update(sys.argv[2], sys.argv[3]))

    elif command == "summary":
        if len(sys.argv) != 3:
            print("Usage: manage_baseline.py summary <baseline_file>")
            sys.exit(1)
        sys.exit(cmd_summary(sys.argv[2]))

    else:
        print(f"Unknown command: {command}")
        print_usage()
        sys.exit(1)


if __name__ == "__main__":
    main()
