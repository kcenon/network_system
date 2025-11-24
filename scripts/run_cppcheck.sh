#!/bin/bash
# BSD 3-Clause License
# Copyright (c) 2024, Network System Project
#
# Local cppcheck analysis script
# Usage: ./scripts/run_cppcheck.sh [--html]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
RESULTS_DIR="$PROJECT_ROOT/static_analysis_results"
XML_REPORT="$RESULTS_DIR/cppcheck_results.xml"
HTML_REPORT_DIR="$RESULTS_DIR/cppcheck_html"
GENERATE_HTML=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --html)
            GENERATE_HTML="yes"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "  Running cppcheck Static Analysis"
echo "=========================================="
echo ""
echo "Project root: $PROJECT_ROOT"
echo "Results dir: $RESULTS_DIR"
echo "HTML report: ${GENERATE_HTML:-disabled}"
echo ""

# Check if cppcheck is available
if ! command -v cppcheck &> /dev/null; then
    echo "Error: cppcheck not found. Please install cppcheck."
    echo "  macOS: brew install cppcheck"
    echo "  Ubuntu: sudo apt-get install cppcheck"
    exit 1
fi

echo "cppcheck version: $(cppcheck --version)"
echo ""

# Create results directory
mkdir -p "$RESULTS_DIR"

# Run cppcheck
echo "--- Running Analysis ---"
cppcheck \
    --enable=all \
    --std=c++20 \
    --suppress=missingIncludeSystem \
    --suppress=unusedFunction \
    --suppress=unmatchedSuppression \
    --inline-suppr \
    --error-exitcode=0 \
    --xml \
    --xml-version=2 \
    -I "$PROJECT_ROOT/include" \
    "$PROJECT_ROOT/include" \
    "$PROJECT_ROOT/src" \
    2> "$XML_REPORT"

echo ""
echo "--- Analysis Summary ---"

if [[ -f "$XML_REPORT" ]]; then
    # Count issues by severity
    echo ""
    echo "Issues by severity:"
    grep -oP 'severity="\K[^"]+' "$XML_REPORT" 2>/dev/null | \
        sort | uniq -c | sort -rn || echo "  No issues found"

    # Count issues by category
    echo ""
    echo "Issues by category:"
    grep -oP 'id="\K[^"]+' "$XML_REPORT" 2>/dev/null | \
        sort | uniq -c | sort -rn | head -15 || echo "  No issues found"

    # Count total issues
    TOTAL=$(grep -c '<error ' "$XML_REPORT" 2>/dev/null || echo "0")
    echo ""
    echo "Total issues: $TOTAL"
fi

# Generate HTML report if requested
if [[ -n "$GENERATE_HTML" ]]; then
    echo ""
    echo "--- Generating HTML Report ---"

    if command -v cppcheck-htmlreport &> /dev/null; then
        cppcheck-htmlreport \
            --file="$XML_REPORT" \
            --report-dir="$HTML_REPORT_DIR" \
            --source-dir="$PROJECT_ROOT"
        echo "HTML report: $HTML_REPORT_DIR/index.html"
    else
        echo "Warning: cppcheck-htmlreport not found. Skipping HTML generation."
        echo "  Install with: pip install cppcheck-htmlreport"
    fi
fi

echo ""
echo "=========================================="
echo "  Results"
echo "=========================================="
echo "XML report: $XML_REPORT"
if [[ -n "$GENERATE_HTML" && -f "$HTML_REPORT_DIR/index.html" ]]; then
    echo "HTML report: $HTML_REPORT_DIR/index.html"
fi
echo ""
echo "=========================================="
echo "  Analysis Complete"
echo "=========================================="

exit 0
