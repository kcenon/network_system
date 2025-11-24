# NET-204: Remove Namespace Duplication

| Field | Value |
|-------|-------|
| **ID** | NET-204 |
| **Title** | Remove Namespace Duplication |
| **Category** | REFACTOR |
| **Priority** | MEDIUM |
| **Status** | DONE |
| **Est. Duration** | 3-4 days |
| **Dependencies** | None |
| **Target Version** | v1.8.0 |

---

## What (Problem Statement)

### Current Problem
The codebase has inconsistent namespace usage:
- Multiple namespace declarations for the same logical group
- Nested namespaces with redundant prefixes
- Some files using `network_system::core::` while others use just `network_system::`
- Inconsistent namespace aliases

### Goal
Unify and simplify namespace structure:
- Single consistent namespace hierarchy
- Remove redundant nesting
- Establish clear namespace conventions
- Document namespace usage guidelines

### Current Namespace Structure (Problematic)
```cpp
// File A
namespace network_system {
namespace core {
    class messaging_server { ... };
}}

// File B
namespace network_system::core {
    class messaging_client { ... };
}

// File C (inconsistent)
namespace network {
namespace system {
    class tcp_socket { ... };
}}
```

### Target Namespace Structure
```cpp
// Consistent C++17 style
namespace network_system::core {
    class messaging_server { ... };
    class messaging_client { ... };
}

namespace network_system::protocols {
    class http_server { ... };
}

namespace network_system::internal {
    class tcp_socket { ... };
}
```

---

## How (Implementation Plan)

### Step 1: Audit Current Namespaces
```bash
# Find all namespace declarations
grep -r "namespace" --include="*.h" --include="*.cpp" src/ include/ | \
    grep -v "using namespace" | sort | uniq > namespace_audit.txt
```

Expected findings:
- `network_system`
- `network_system::core`
- `network_system::protocols`
- `network_system::internal`
- `network_system::utils`
- Any inconsistent variants

### Step 2: Define Target Hierarchy
```
network_system/
├── core/           # Core messaging classes
│   ├── messaging_server
│   ├── messaging_client
│   ├── secure_messaging_server
│   └── secure_messaging_client
├── protocols/      # Protocol implementations
│   ├── http_server
│   ├── http_client
│   ├── websocket_server
│   └── websocket_client
├── internal/       # Internal implementation details
│   ├── tcp_socket
│   ├── udp_socket
│   └── pipeline
└── utils/          # Utility classes
    ├── metrics_collector
    └── health_monitor
```

### Step 3: Create Migration Script
```python
#!/usr/bin/env python3
# scripts/migrate_namespaces.py

import re
import os
from pathlib import Path

MIGRATIONS = {
    # Old pattern -> New pattern
    r'namespace network\s*\{\s*namespace system': 'namespace network_system',
    r'namespace network_system\s*\{\s*namespace core': 'namespace network_system::core',
    r'using namespace network::system': 'using namespace network_system',
}

def migrate_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    original = content
    for old, new in MIGRATIONS.items():
        content = re.sub(old, new, content)

    if content != original:
        print(f"Migrating: {filepath}")
        with open(filepath, 'w') as f:
            f.write(content)

def main():
    for ext in ['*.h', '*.hpp', '*.cpp']:
        for filepath in Path('.').rglob(ext):
            if 'build' not in str(filepath):
                migrate_file(filepath)

if __name__ == '__main__':
    main()
```

### Step 4: Update Header Files
```cpp
// Before: include/network_system/core/messaging_server.h
namespace network_system {
namespace core {

class messaging_server {
    // ...
};

} // namespace core
} // namespace network_system

// After: include/network_system/core/messaging_server.h
namespace network_system::core {

class messaging_server {
    // ...
};

} // namespace network_system::core
```

### Step 5: Update Forward Declarations
```cpp
// Before
namespace network_system { namespace core { class messaging_server; }}

// After
namespace network_system::core { class messaging_server; }
```

### Step 6: Add Namespace Aliases (if needed)
```cpp
// include/network_system/namespace.h

namespace network_system {

// Convenience aliases for common namespaces
namespace ns = network_system;
namespace core = network_system::core;
namespace proto = network_system::protocols;

} // namespace network_system
```

---

## Test (Verification Plan)

### Compilation Test
```bash
# Full rebuild to catch any namespace errors
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . 2>&1 | tee build.log

# Check for namespace-related errors
grep -i "namespace" build.log | grep -i "error"
```

### Unit Tests
```cpp
TEST(Namespace, ConsistentDeclaration) {
    // Verify classes are accessible via expected namespaces
    network_system::core::messaging_server server("test");
    network_system::core::messaging_client client("client");
    network_system::protocols::http_server http;

    SUCCEED();
}

TEST(Namespace, NoAmbiguity) {
    using namespace network_system;
    using namespace network_system::core;

    // Should compile without ambiguity
    messaging_server server("test");
    messaging_client client("client");

    SUCCEED();
}
```

### API Compatibility Test
```cpp
// Verify old code still works (if backward compatibility needed)
TEST(Namespace, BackwardCompatibility) {
    // If we provide aliases, test them
    namespace ns = network_system;

    ns::core::messaging_server server("test");
    SUCCEED();
}
```

### Manual Verification
1. Grep for any remaining old-style namespace declarations
2. Review all public headers for consistency
3. Check documentation examples match new namespaces

---

## Acceptance Criteria

- [ ] All namespace declarations use C++17 nested style (`A::B::C`)
- [ ] No duplicate or inconsistent namespace patterns
- [ ] All files follow the defined hierarchy
- [ ] Migration script documented and tested
- [ ] All unit tests pass
- [ ] Documentation updated with new namespace conventions
- [ ] Backward compatibility maintained (if required)

---

## Notes

- This is a breaking change if not providing aliases
- Consider providing deprecated aliases for transition period
- Update all example code in documentation
- IDE/editor configurations may need updating
