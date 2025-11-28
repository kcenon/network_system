# Static Analysis Guide

This guide explains how to run static analysis tools for the Network System library.

## Quick Start

```bash
# Run clang-tidy analysis
./scripts/run_clang_tidy.sh

# Run cppcheck analysis
./scripts/run_cppcheck.sh

# Run with automatic fixes (clang-tidy only)
./scripts/run_clang_tidy.sh --fix

# Generate HTML report (cppcheck)
./scripts/run_cppcheck.sh --html
```

## Available Tools

### 1. clang-tidy

A clang-based C++ linter tool that provides diagnostics and fixes for typical programming errors.

**Configuration**: `.clang-tidy` at project root

**Enabled checks**:
- `bugprone-*`: Bug-prone code patterns
- `concurrency-*`: Concurrency issues
- `modernize-*`: Modern C++ style
- `performance-*`: Performance improvements
- `readability-*`: Code readability
- `cppcoreguidelines-*`: C++ Core Guidelines

**Running locally**:
```bash
# Basic analysis
./scripts/run_clang_tidy.sh

# With automatic fixes
./scripts/run_clang_tidy.sh --fix
```

**Results**: `static_analysis_results/` directory

### 2. cppcheck

A static analysis tool for C/C++ code that detects bugs and undefined behavior.

**Configuration**: `.cppcheck` at project root

**Running locally**:
```bash
# Basic analysis
./scripts/run_cppcheck.sh

# With HTML report
./scripts/run_cppcheck.sh --html
```

**Results**: `static_analysis_results/cppcheck_results.xml`

## Baseline Management

Use the baseline management script to track warning changes over time.

### Check Against Baseline

```bash
# Check clang-tidy output
./scripts/manage_baseline.py check clang_tidy_output.txt baseline.json

# Check cppcheck output
./scripts/manage_baseline.py check cppcheck_results.xml baseline.json
```

### Update Baseline

```bash
# Update after fixing issues
./scripts/manage_baseline.py update clang_tidy_output.txt baseline.json
```

### View Baseline Summary

```bash
./scripts/manage_baseline.py summary baseline.json
```

## CI Integration

Static analysis runs automatically on:
- Push to `main` or `phase-*` branches
- Pull requests to `main`

See `.github/workflows/static-analysis.yml` for configuration.

### CI Jobs

1. **clang-tidy**: Analyzes all header files
2. **cppcheck**: Runs full analysis on include and src directories

### Artifacts

CI generates and uploads:
- `clang-tidy-baseline`: Analysis results and summary
- `cppcheck-baseline`: XML report and summary

## Adding Suppressions

### clang-tidy

Add to `.clang-tidy`:
```yaml
Checks: >
  -check-to-disable,
```

Or inline in code:
```cpp
// NOLINTBEGIN(check-name)
// code...
// NOLINTEND(check-name)
```

### cppcheck

Inline suppression:
```cpp
// cppcheck-suppress warningId
problematic_code();
```

Or in `.cppcheck`:
```xml
<suppress>
  <id>warningId</id>
  <fileName>path/to/file.cpp</fileName>
</suppress>
```

## Best Practices

1. **Run locally before committing**: Catch issues early
2. **Don't ignore all warnings**: Only suppress with good reason
3. **Document suppressions**: Add comments explaining why
4. **Update baseline regularly**: After fixing issues
5. **Review CI results**: Check artifacts for details

## Troubleshooting

### "compile_commands.json not found"

Generate it with CMake:
```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

### "clang-tidy not found"

Install clang-tidy:
```bash
# macOS
brew install llvm
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"

# Ubuntu
sudo apt-get install clang-tidy
```

### "cppcheck not found"

Install cppcheck:
```bash
# macOS
brew install cppcheck

# Ubuntu
sudo apt-get install cppcheck
```

### Too many false positives

1. Review `.clang-tidy` configuration
2. Add specific suppressions
3. Update baseline if necessary

## Further Reading

- [clang-tidy documentation](https://clang.llvm.org/extra/clang-tidy/)
- [cppcheck manual](http://cppcheck.net/manual.html)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
