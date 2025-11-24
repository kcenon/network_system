# NET-302: Automate Static Analysis

| Field | Value |
|-------|-------|
| **ID** | NET-302 |
| **Title** | Automate Static Analysis |
| **Category** | REFACTOR |
| **Priority** | LOW |
| **Status** | TODO |
| **Est. Duration** | 2-3 days |
| **Dependencies** | None |
| **Target Version** | v1.9.0 |

---

## What (Problem Statement)

### Current Problem
Static analysis is not automated in the development workflow:
- No CI integration for code analysis
- Manual clang-tidy runs are inconsistent
- No baseline for expected warnings
- Style issues caught late in review process

### Goal
Automate static analysis tools in CI pipeline:
- Integrate clang-tidy with CI
- Add cppcheck analysis
- Configure include-what-you-use
- Establish warning baseline
- Auto-fix capabilities where safe

### Benefits
- Catch bugs earlier in development
- Consistent code quality enforcement
- Reduced manual review burden
- Documented code quality standards

---

## How (Implementation Plan)

### Step 1: Configure clang-tidy

#### Create Configuration File
```yaml
# .clang-tidy

Checks: >
  -*,
  bugprone-*,
  cert-*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  misc-*,
  modernize-*,
  performance-*,
  portability-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers,
  -cppcoreguidelines-avoid-magic-numbers,
  -readability-identifier-length,
  -cppcoreguidelines-pro-type-reinterpret-cast,
  -cppcoreguidelines-pro-bounds-pointer-arithmetic

WarningsAsErrors: ''

HeaderFilterRegex: 'include/network_system/.*'

CheckOptions:
  - key: readability-identifier-naming.ClassCase
    value: lower_case
  - key: readability-identifier-naming.FunctionCase
    value: lower_case
  - key: readability-identifier-naming.VariableCase
    value: lower_case
  - key: readability-identifier-naming.PrivateMemberSuffix
    value: _
  - key: readability-identifier-naming.ConstantCase
    value: lower_case
  - key: readability-identifier-naming.EnumConstantCase
    value: lower_case
  - key: modernize-use-override.OverrideSpelling
    value: override
  - key: performance-unnecessary-value-param.AllowedTypes
    value: 'std::function.*'
```

### Step 2: Create Analysis Scripts

```bash
#!/bin/bash
# scripts/run_clang_tidy.sh

set -e

BUILD_DIR="${BUILD_DIR:-build}"
FIXES_FILE="clang_tidy_fixes.yaml"

echo "=== Running clang-tidy ==="

# Ensure compile_commands.json exists
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "Generating compile_commands.json..."
    cmake -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

# Find source files
SOURCE_FILES=$(find src include -name "*.cpp" -o -name "*.h" | grep -v build)

# Run clang-tidy
run-clang-tidy \
    -p "$BUILD_DIR" \
    -header-filter="include/network_system/.*" \
    -export-fixes="$FIXES_FILE" \
    $SOURCE_FILES

# Report summary
if [ -f "$FIXES_FILE" ]; then
    FIXES_COUNT=$(grep -c "DiagnosticName:" "$FIXES_FILE" || echo 0)
    echo ""
    echo "=== Summary ==="
    echo "Found $FIXES_COUNT issues"

    if [ "$FIXES_COUNT" -gt 0 ]; then
        echo ""
        echo "To apply fixes automatically:"
        echo "  clang-apply-replacements $FIXES_FILE"
    fi
fi
```

```bash
#!/bin/bash
# scripts/run_cppcheck.sh

set -e

echo "=== Running cppcheck ==="

cppcheck \
    --enable=all \
    --suppress=missingIncludeSystem \
    --suppress=unmatchedSuppression \
    --inline-suppr \
    --error-exitcode=1 \
    --xml \
    --xml-version=2 \
    --output-file=cppcheck_report.xml \
    -I include \
    src/

# Generate HTML report (if cppcheck-htmlreport is available)
if command -v cppcheck-htmlreport &> /dev/null; then
    cppcheck-htmlreport \
        --file=cppcheck_report.xml \
        --report-dir=cppcheck_html \
        --source-dir=.
    echo "HTML report: cppcheck_html/index.html"
fi
```

### Step 3: Create CI Workflow

```yaml
# .github/workflows/static-analysis.yml

name: Static Analysis

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  clang-tidy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y clang-tidy clang-tools

      - name: Configure CMake
        run: cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

      - name: Run clang-tidy
        run: |
          ./scripts/run_clang_tidy.sh 2>&1 | tee clang_tidy_output.txt
          # Fail if there are errors (not warnings)
          if grep -q "error:" clang_tidy_output.txt; then
            exit 1
          fi

      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: clang-tidy-results
          path: |
            clang_tidy_output.txt
            clang_tidy_fixes.yaml

  cppcheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install cppcheck
        run: sudo apt-get install -y cppcheck

      - name: Run cppcheck
        run: ./scripts/run_cppcheck.sh

      - name: Upload report
        uses: actions/upload-artifact@v4
        with:
          name: cppcheck-report
          path: cppcheck_report.xml

  include-what-you-use:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install IWYU
        run: sudo apt-get install -y iwyu

      - name: Configure with IWYU
        run: |
          cmake -B build \
            -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE="iwyu;-Xiwyu;--mapping_file=iwyu.imp"

      - name: Build and check includes
        run: cmake --build build 2>&1 | tee iwyu_output.txt

      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: iwyu-results
          path: iwyu_output.txt
```

### Step 4: Create Baseline Management

```python
#!/usr/bin/env python3
# scripts/manage_baseline.py

import json
import sys
from pathlib import Path

def load_baseline(path):
    if not Path(path).exists():
        return {"warnings": [], "version": 1}
    with open(path) as f:
        return json.load(f)

def save_baseline(data, path):
    with open(path, 'w') as f:
        json.dump(data, f, indent=2)

def parse_clang_tidy_output(output_path):
    warnings = []
    with open(output_path) as f:
        for line in f:
            if ": warning:" in line or ": error:" in line:
                parts = line.split(":")
                if len(parts) >= 4:
                    warnings.append({
                        "file": parts[0],
                        "line": int(parts[1]),
                        "message": ":".join(parts[3:]).strip()
                    })
    return warnings

def compare_with_baseline(current, baseline):
    baseline_set = {(w["file"], w["message"]) for w in baseline["warnings"]}
    current_set = {(w["file"], w["message"]) for w in current}

    new_warnings = current_set - baseline_set
    fixed_warnings = baseline_set - current_set

    return new_warnings, fixed_warnings

def main():
    if len(sys.argv) < 3:
        print("Usage: manage_baseline.py <command> <args>")
        print("Commands:")
        print("  check <output> <baseline>  - Check against baseline")
        print("  update <output> <baseline> - Update baseline")
        sys.exit(1)

    command = sys.argv[1]

    if command == "check":
        output_path, baseline_path = sys.argv[2], sys.argv[3]
        current = parse_clang_tidy_output(output_path)
        baseline = load_baseline(baseline_path)

        new_warnings, fixed = compare_with_baseline(current, baseline)

        if new_warnings:
            print(f"ERROR: {len(new_warnings)} new warnings introduced!")
            for w in new_warnings:
                print(f"  - {w[0]}: {w[1]}")
            sys.exit(1)

        if fixed:
            print(f"INFO: {len(fixed)} warnings fixed! Consider updating baseline.")

        print("OK: No new warnings")

    elif command == "update":
        output_path, baseline_path = sys.argv[2], sys.argv[3]
        current = parse_clang_tidy_output(output_path)
        baseline = {"warnings": current, "version": 1}
        save_baseline(baseline, baseline_path)
        print(f"Baseline updated with {len(current)} warnings")

if __name__ == "__main__":
    main()
```

---

## Test (Verification Plan)

### Local Testing
```bash
# Test clang-tidy script
./scripts/run_clang_tidy.sh

# Test cppcheck script
./scripts/run_cppcheck.sh

# Test baseline management
./scripts/manage_baseline.py check clang_tidy_output.txt baseline.json
```

### CI Testing
1. Create PR with intentional warning
2. Verify CI fails
3. Fix warning
4. Verify CI passes

### Verification Checklist
- [ ] clang-tidy runs without errors
- [ ] cppcheck produces valid XML
- [ ] CI workflow triggers on PR
- [ ] Baseline comparison works
- [ ] HTML reports generate correctly

---

## Acceptance Criteria

- [ ] .clang-tidy configuration file created
- [ ] CI workflow for clang-tidy running
- [ ] CI workflow for cppcheck running
- [ ] Baseline management script working
- [ ] Documentation on adding suppressions
- [ ] All existing code passes analysis
- [ ] IWYU integration (optional)

---

## Notes

- Start with warning mode, move to error mode gradually
- Some checks may need project-specific suppressions
- Update baseline when fixing existing issues
- Consider pre-commit hooks for local checking
