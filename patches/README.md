# Patches Directory

This directory contains patches for upstream dependencies that need fixes to work correctly with network_system.

## Available Patches

### thread_system_fmt_fix.patch

**Purpose**: Fixes missing FMT header include in thread_system's `formatter_macros.h`

**Target**: thread_system (https://github.com/kcenon/thread_system)

**Issue**: The `DECLARE_FORMATTER` macro uses `fmt::formatter` but doesn't explicitly include `<fmt/format.h>`, causing compilation errors when `USE_STD_FORMAT` is not defined.

**Fix**: Adds explicit include of `<fmt/format.h>` when not using std::format:
```cpp
#ifndef USE_STD_FORMAT
#include <fmt/format.h>
#endif
```

**Application**: This patch is automatically applied by `.github/actions/setup-thread-system/action.yml` during CI builds.

**Manual Application**:
```bash
cd /path/to/thread_system
git apply /path/to/network_system/patches/thread_system_fmt_fix.patch
```

**Upstream Status**: Should be reported to thread_system project for permanent fix.

---

## Patch Management Guidelines

### When to Add a Patch

Add a patch when:
1. An upstream dependency has a bug that blocks network_system builds
2. The upstream project hasn't released a fix yet
3. The fix is simple and non-invasive
4. The patch can be automatically applied in CI

### Patch Naming Convention

Use format: `<dependency>_<issue_description>.patch`

Examples:
- `thread_system_fmt_fix.patch`
- `asio_windows_build_fix.patch`

### Patch Documentation

Each patch must be documented:
1. Add entry to this README
2. Document in `KNOWN_ISSUES.md` under "Upstream Issues"
3. Include comments in the patch file explaining the fix
4. Update `.github/actions/setup-<dependency>/action.yml` to apply it

### Removing Patches

Remove a patch when:
1. Upstream dependency releases a fix
2. We upgrade to a version that includes the fix
3. The workaround is no longer needed

Steps to remove:
1. Delete the patch file
2. Remove patch application from GitHub Actions
3. Update `KNOWN_ISSUES.md` to mark as resolved
4. Update this README

---

**Maintainer**: kcenon
**Last Updated**: 2025-11-03
