#!/bin/bash
# BSD 3-Clause License
# Copyright (c) 2024, Network System Project
#
# AddressSanitizer build and run script
# Usage: ./scripts/run_with_asan.sh [target] [args...]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build_asan}"
TARGET="${1:-network_memory_profile_demo}"
shift 2>/dev/null || true
ARGS="$*"

echo "=========================================="
echo "  Building with AddressSanitizer"
echo "=========================================="
echo ""
echo "Project root: $PROJECT_ROOT"
echo "Build dir: $BUILD_DIR"
echo "Target: $TARGET"
echo ""

# Configure with ASAN
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "Configuring with ASAN..."
    cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
        -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
        -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address"
fi

# Build target
echo ""
echo "Building $TARGET..."
cmake --build "$BUILD_DIR" --target "$TARGET" -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Find executable
EXECUTABLE=""
if [[ -f "$BUILD_DIR/bin/$TARGET" ]]; then
    EXECUTABLE="$BUILD_DIR/bin/$TARGET"
elif [[ -f "$BUILD_DIR/bin/tests/$TARGET" ]]; then
    EXECUTABLE="$BUILD_DIR/bin/tests/$TARGET"
elif [[ -f "$BUILD_DIR/$TARGET" ]]; then
    EXECUTABLE="$BUILD_DIR/$TARGET"
fi

if [[ -z "$EXECUTABLE" ]]; then
    echo "Error: Could not find executable for $TARGET"
    exit 1
fi

echo ""
echo "=========================================="
echo "  Running with ASAN"
echo "=========================================="
echo ""
echo "Executable: $EXECUTABLE"
echo "Args: $ARGS"
echo ""

# Run with ASAN options
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:print_stats=1:check_initialization_order=1"
"$EXECUTABLE" $ARGS

echo ""
echo "=========================================="
echo "  ASAN Run Complete"
echo "=========================================="
