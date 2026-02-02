# Refactoring Documentation

> **Epic**: #577 - Apply Facade pattern to reduce protocol header complexity
> **Issue**: #610 - Move protocol implementation headers to internal
> **Goal**: Reduce public headers from 153 to < 40

## Overview

This directory contains documentation for the ongoing refactoring effort to simplify the network_system public API by hiding implementation details behind facade interfaces.

## Documents

| Document | Purpose | Audience |
|----------|---------|----------|
| [HEADER_AUDIT_PHASE1.md](HEADER_AUDIT_PHASE1.md) | Complete audit of all 153 public headers with categorization | Developers, Maintainers |
| [MIGRATION_GUIDE_V2.md](MIGRATION_GUIDE_V2.md) | Step-by-step migration guide from v1.x to v2.0 facade API | Library Users |

## Refactoring Phases

### Phase 1: Preparation (Current)
**Goal**: Set foundation for migration without breaking changes
**Status**: ✅ In Progress (PR #XXX)

- [x] Create comprehensive header audit
- [x] Write migration guide with examples
- [x] Add deprecation warnings to headers
- [ ] Update examples to use facade API
- [ ] Document in this README

### Phase 2: Move Protocol Implementations (4 PRs)
**Goal**: Move concrete client/server implementations to internal

- [ ] PR 1: Move TCP implementations (`messaging_client.h`, `messaging_server.h`, etc.)
- [ ] PR 2: Move UDP implementations
- [ ] PR 3: Move HTTP/WebSocket implementations
- [ ] PR 4: Move QUIC implementations

### Phase 3: Move Protocol Details (3 PRs)
**Goal**: Move low-level protocol implementations to internal

- [ ] PR 5: Move HTTP2 protocol internals
- [ ] PR 6: Move gRPC protocol internals
- [ ] PR 7: Move utility internals

### Phase 4: Cleanup (1 PR)
**Goal**: Finalize public API and remove deprecated code

- [ ] Remove deprecated headers
- [ ] Final documentation update
- [ ] Verify < 40 public headers achieved
- [ ] Update CHANGELOG for v2.0 release

## Public API Surface (Target: 35 headers)

### Core API (Keep Public)
1. **Facades** (4): `tcp_facade.h`, `udp_facade.h`, `http_facade.h`, `websocket_facade.h`
2. **Interfaces** (14): `i_protocol_client.h`, `i_protocol_server.h`, etc.
3. **Integration** (13): `network_system_bridge.h`, adapters, etc.
4. **Utilities** (4): `network_system.h`, `network_context.h`, etc.

**Total**: ~35 public headers

## Migration Timeline

| Version | Release | Status | Migration Action |
|---------|---------|--------|------------------|
| v1.x | Current | Stable | No action needed |
| v2.0-beta | Q1 2026 | Testing | Opt-in migration with `NETWORK_SYSTEM_EXPOSE_INTERNALS=ON` |
| v2.0 | Q2 2026 | Stable | Migrate to facade API (recommended) |
| v2.x | Ongoing | Maintained | Compatibility mode with deprecation warnings |
| v3.0 | 2027 | Future | Facade-only API, internal headers not accessible |

## Quick Start for Users

### If you're using v1.x
1. Read [MIGRATION_GUIDE_V2.md](MIGRATION_GUIDE_V2.md)
2. Update your code to use facade API
3. Test with v2.0-beta when available
4. Migrate fully before v3.0 (2027)

### If you're a maintainer
1. Read [HEADER_AUDIT_PHASE1.md](HEADER_AUDIT_PHASE1.md) for categorization
2. Follow phase plan for incremental refactoring
3. Ensure each PR maintains backward compatibility during transition
4. Update documentation as headers move

## Benefits of Facade-First API

### For Users
✅ **Simpler Code**: No template parameters or protocol tags
✅ **Faster Compilation**: Fewer header dependencies
✅ **Easier Upgrades**: Implementation changes don't break your code
✅ **Better Documentation**: Clear, focused public API

### For Maintainers
✅ **Faster Iteration**: Can change internals without affecting users
✅ **Clearer Responsibilities**: Public API vs. implementation separation
✅ **Easier Testing**: Mock interfaces, not concrete implementations
✅ **Better Modularity**: Internal code can be reorganized freely

## Compatibility During Transition

### Option 1: Immediate Migration (Recommended)
Migrate to facade API immediately for best long-term results.

### Option 2: Gradual Migration
Enable compatibility mode:
```cmake
set(NETWORK_SYSTEM_EXPOSE_INTERNALS ON)
```

This allows old code to continue working while you migrate incrementally.

### Option 3: Advanced Access (Not Recommended)
Explicitly include internal headers if absolutely necessary:
```cpp
#include "src/internal/tcp/messaging_client.h"  // Not recommended
```

## FAQ

### Q: Will my v1.x code break in v2.0?
**A**: Not immediately. Enable `NETWORK_SYSTEM_EXPOSE_INTERNALS` for compatibility mode during transition.

### Q: When must I migrate?
**A**: Before v3.0 (estimated 2027). We recommend migrating to v2.0 in Q2 2026.

### Q: What if I need direct access to implementations?
**A**: Most use cases are covered by facade API. If not, please open an issue describing your use case.

### Q: How do I suppress deprecation warnings?
**A**: Define `NETWORK_SYSTEM_SUPPRESS_DEPRECATION_WARNINGS` (but you should migrate instead).

## Contributing

If you're contributing to this refactoring:
1. Follow the phase plan
2. Maintain backward compatibility in each PR
3. Update both audit and migration docs
4. Add deprecation warnings before moving headers
5. Ensure all tests pass

## Related Links

- **Epic #577**: https://github.com/kcenon/network_system/issues/577
- **Issue #610**: https://github.com/kcenon/network_system/issues/610
- **Facade Documentation**: See header comments in `include/kcenon/network/facade/`

---

**Last Updated**: 2026-02-02
**Maintained By**: @kcenon
**Questions**: Tag issue #610
