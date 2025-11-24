#!/bin/bash
# BSD 3-Clause License
# Copyright (c) 2024, Network System Project
#
# Memory profiling script for Network System
# Usage: ./scripts/run_memory_profile.sh [mode]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
SAMPLE="network_memory_profile_demo"
MODE="${1:-all}"

echo "=========================================="
echo "  Network System Memory Profiling"
echo "=========================================="
echo ""
echo "Project root: $PROJECT_ROOT"
echo "Build dir: $BUILD_DIR"
echo "Mode: $MODE"
echo ""

# Check if sample exists
if [[ ! -f "$BUILD_DIR/bin/$SAMPLE" ]]; then
    echo "Building memory profile demo..."
    cmake --build "$BUILD_DIR" --target "$SAMPLE"
fi

echo ""
echo "--- Native Memory Tracking ---"
"$BUILD_DIR/bin/$SAMPLE" "$MODE"

# Option: Valgrind (if available, Linux only)
if command -v valgrind &> /dev/null; then
    echo ""
    echo "--- Valgrind Memcheck ---"
    echo "(Note: This may take a while...)"
    valgrind --leak-check=full \
             --show-leak-kinds=definite,possible \
             --errors-for-leak-kinds=definite \
             --track-origins=yes \
             "$BUILD_DIR/bin/$SAMPLE" connections 2>&1 | head -100
fi

# Option: Heaptrack (if available, Linux only)
if command -v heaptrack &> /dev/null; then
    echo ""
    echo "--- Heaptrack ---"
    heaptrack "$BUILD_DIR/bin/$SAMPLE" connections
    echo "Run 'heaptrack_gui heaptrack.$SAMPLE.*.gz' to view results"
fi

echo ""
echo "=========================================="
echo "  Profiling Complete"
echo "=========================================="
