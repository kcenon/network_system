# Known Issues

**Last Updated**: 2025-11-03

---

## 🐛 Test Build Issues

### container_system FMT Linkage (Medium Priority)

**Status**: 🟡 **Known Issue** - Does not affect library build

**Description**:
Test executables fail to link due to missing FMT symbols from `libcontainer_system.a`. The container_system library uses FMT formatting but does not include FMT symbols in its static library.

**Error**:
```
Undefined symbols for architecture arm64:
  "fmt::v11::detail::vformat_to(...)", referenced from:
      utility_module::formatter::format_to<...> in libcontainer_system.a
```

**Impact**:
- ❌ Test executables do not build
- ✅ NetworkSystem library (18MB) builds successfully
- ✅ Production code fully functional
- ✅ thread_system integration complete

**Root Cause**:
Static libraries (`*.a` files) do not include their dependencies. When `libcontainer_system.a` is linked into test executables, the FMT symbols it references must be provided by the final executable's link command.

**Possible Solutions**:

1. **Rebuild container_system with FMT** (Recommended)
   ```bash
   cd /Users/dongcheolshin/Sources/container_system
   cmake -DBUILD_WITH_FMT=ON -B build
   cmake --build build
   ```

2. **Use FMT header-only mode**
   - Set `FMT_HEADER_ONLY` define
   - May increase compile times

3. **Link FMT explicitly in container_system**
   - Modify container_system's CMakeLists.txt
   - Add `PUBLIC` linkage for FMT

4. **Bypass container_system in tests** (Temporary)
   - Build tests without container_system integration
   - Reduces test coverage but allows testing core functionality

**Workaround**:
The NetworkSystem library itself is production-ready. Tests can be temporarily disabled or built without container_system integration until container_system is rebuilt with proper FMT linkage.

**Timeline**:
- Non-blocking for thread_system integration ✅
- Can be addressed when container_system is next updated
- Estimated fix time: 30 minutes (rebuild container_system)

---

## ✅ Resolved Issues

### typed_thread_pool Linker Errors (2025-11-03)

**Status**: ✅ **RESOLVED**

**Solution**: Implemented Phase 3 fallback using regular `thread_pool` instead of `typed_thread_pool_t<pipeline_priority>`. This avoids template instantiation issues while maintaining clean API for future upgrade.

### compatibility.h Obsolete Classes (2025-11-03)

**Status**: ✅ **RESOLVED**

**Solution**: Updated `compatibility.h` to use `thread_pool_manager` instead of removed Phase 4 classes (`thread_pool_interface`, `basic_thread_pool`, `thread_integration_manager`).

---

## 📋 Not Issues (By Design)

### Phase 3 Uses Regular thread_pool

**Description**: Pipeline pool uses regular `thread_pool` instead of `typed_thread_pool`.

**Rationale**:
- `typed_thread_pool_t<custom_enum>` requires explicit instantiation in thread_system
- Fallback approach is simpler and more maintainable
- Priority can be tracked via logging
- API designed for seamless future upgrade
- No performance impact for network workloads

**Status**: ✅ **Working As Designed**

---

## 🔍 Upstream Issues (thread_system)

### FMT Header Not Explicitly Included in formatter_macros.h

**Status**: ✅ **PATCHED** - Automatic patch applied during CI builds

**Description**:
The thread_system's `formatter_macros.h` uses `fmt::formatter` but doesn't explicitly include `<fmt/format.h>`. This causes compilation errors when `USE_STD_FORMAT` is not defined.

**Error Messages** (before patch):
```
error: 'fmt' has not been declared
error: expected unqualified-id before '<' token
```

**Affected Files in thread_system**:
- `include/kcenon/thread/utils/formatter_macros.h` (needs patch)
- `include/kcenon/thread/core/job.h` (uses DECLARE_FORMATTER)
- `include/kcenon/thread/core/job_queue.h` (uses DECLARE_FORMATTER)
- `include/kcenon/thread/core/thread_conditions.h` (uses DECLARE_FORMATTER)

**Root Cause**:
The `DECLARE_FORMATTER` macro in `formatter_macros.h` expands to code using `fmt::formatter`, but the file only includes `formatter.h` which conditionally includes `<fmt/format.h>` using `__has_include`. This conditional include doesn't guarantee `fmt` namespace availability in all build contexts.

**Our Solution**:
✅ **Automatic patch applied during CI builds**

A patch file is maintained in `patches/thread_system_fmt_fix.patch` that adds the missing include:

```cpp
// Ensure fmt types are available when using fmt library
#ifndef USE_STD_FORMAT
#include <fmt/format.h>
#endif
```

This patch is automatically applied by our GitHub Actions composite action (`.github/actions/setup-thread-system/action.yml`) before building thread_system.

**Alternative Workarounds** (if not using our CI):

1. **Apply the patch manually**:
   ```bash
   cd /path/to/thread_system
   git apply /path/to/network_system/patches/thread_system_fmt_fix.patch
   ```

2. **Define USE_STD_FORMAT** (if C++20 `<format>` available):
   ```cmake
   add_compile_definitions(USE_STD_FORMAT)
   ```

3. **Ensure FMT is installed** (required regardless):
   ```yaml
   # Ubuntu
   sudo apt-get install -y libfmt-dev

   # macOS
   brew install fmt
   ```

**Impact on network_system**:
- ✅ **Fully resolved** - Patch applied automatically in CI
- ✅ CI workflows build successfully
- ✅ All platforms supported (Linux, macOS, Windows)

**Upstream Status**:
- 🟡 Should be reported to thread_system project
- 🟡 Upstream fix would eliminate need for our patch
- ✅ Not blocking our development (patch works reliably)

---

## 🔍 Investigation Needed

None at this time.

---

## 📞 Reporting Issues

If you encounter issues:

1. Check this document first
2. Verify NetworkSystem library builds: `ls -lh build/libNetworkSystem.a`
3. Check if issue is test-specific or library-specific
4. Report with:
   - CMake version
   - Platform (macOS/Linux/Windows)
   - Full error message
   - Steps to reproduce

---

**Document Version**: 1.0
**Maintainer**: kcenon
**Last Review**: 2025-11-03
