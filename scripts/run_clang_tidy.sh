#!/bin/bash
# BSD 3-Clause License
# Copyright (c) 2024, Network System Project
#
# Local clang-tidy analysis script
# Usage: ./scripts/run_clang_tidy.sh [--fix]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
FIXES_FILE="$PROJECT_ROOT/clang_tidy_fixes.yaml"
FIX_MODE=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --fix)
            FIX_MODE="--fix"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "  Running clang-tidy Static Analysis"
echo "=========================================="
echo ""
echo "Project root: $PROJECT_ROOT"
echo "Build dir: $BUILD_DIR"
echo "Fix mode: ${FIX_MODE:-disabled}"
echo ""

# Ensure compile_commands.json exists
if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
    echo "Generating compile_commands.json..."
    cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null; then
    echo "Error: clang-tidy not found. Please install clang-tidy."
    exit 1
fi

echo "clang-tidy version: $(clang-tidy --version | head -1)"
echo ""

# Find source files (exclude build directories)
SOURCE_FILES=$(find "$PROJECT_ROOT/include" "$PROJECT_ROOT/src" \
    -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
    2>/dev/null | grep -v "/build" | grep -v "/_deps/" | sort)

FILE_COUNT=$(echo "$SOURCE_FILES" | wc -l | tr -d ' ')
echo "Found $FILE_COUNT files to analyze"
echo ""

# Create output directory for results
RESULTS_DIR="$PROJECT_ROOT/static_analysis_results"
mkdir -p "$RESULTS_DIR"

# Run clang-tidy
echo "--- Running Analysis ---"
WARNINGS=0
ERRORS=0

for file in $SOURCE_FILES; do
    REL_PATH=$(echo "$file" | sed "s|$PROJECT_ROOT/||")
    echo -n "Analyzing: $REL_PATH... "

    OUTPUT=$(clang-tidy "$file" -p="$BUILD_DIR" $FIX_MODE 2>&1) || true

    # Count warnings and errors
    FILE_WARNINGS=$(echo "$OUTPUT" | grep -c "warning:" || true)
    FILE_ERRORS=$(echo "$OUTPUT" | grep -c "error:" || true)

    if [[ $FILE_ERRORS -gt 0 ]]; then
        echo "ERRORS: $FILE_ERRORS"
        ERRORS=$((ERRORS + FILE_ERRORS))
    elif [[ $FILE_WARNINGS -gt 0 ]]; then
        echo "WARNINGS: $FILE_WARNINGS"
        WARNINGS=$((WARNINGS + FILE_WARNINGS))
    else
        echo "OK"
    fi

    # Save detailed output if there are issues
    if [[ $FILE_WARNINGS -gt 0 || $FILE_ERRORS -gt 0 ]]; then
        SAFE_NAME=$(echo "$REL_PATH" | tr '/' '_')
        echo "$OUTPUT" >> "$RESULTS_DIR/${SAFE_NAME}.log"
    fi
done

echo ""
echo "=========================================="
echo "  Analysis Summary"
echo "=========================================="
echo "Files analyzed: $FILE_COUNT"
echo "Total warnings: $WARNINGS"
echo "Total errors: $ERRORS"
echo "Results saved to: $RESULTS_DIR/"
echo ""

# Generate summary by category
if [[ -d "$RESULTS_DIR" && $(ls -A "$RESULTS_DIR" 2>/dev/null) ]]; then
    echo "--- Warnings by Category ---"
    cat "$RESULTS_DIR"/*.log 2>/dev/null | \
        grep "warning:" | \
        sed 's/.*\[\(.*\)\]/\1/' | \
        sort | uniq -c | sort -rn | head -20
fi

echo ""
echo "=========================================="
echo "  Analysis Complete"
echo "=========================================="

# Exit with error if there are errors
if [[ $ERRORS -gt 0 ]]; then
    echo ""
    echo "ERROR: $ERRORS error(s) found. Please fix them before committing."
    exit 1
fi

exit 0
