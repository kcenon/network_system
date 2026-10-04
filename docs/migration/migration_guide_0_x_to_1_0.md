---
doc_id: "NET-MIGR-007"
doc_title: "Migration Guide: 0.x to 1.0"
doc_version: "1.0.0"
doc_date: "2026-05-10"
doc_status: "Released"
project: "network_system"
category: "MIGR"
issue: "1129"
parent_epic: "964"
---

# Migration Guide: 0.x to 1.0

> **SSOT**: This document is the single source of truth for migrating downstream
> consumers from any `network_system` 0.x version to the v1.0 stable release.

> Tracking issue: [#1129](https://github.com/kcenon/network_system/issues/1129).
> Part of [#964](https://github.com/kcenon/network_system/issues/964).

## TL;DR — Quick Checklist

For most downstream consumers, the migration to v1.0 is a no-op at the source
level. The v1.0 freeze deliberately introduced **zero breaking changes**.

- [ ] Confirm CMake target spelling is `network_system::network_system`
      (the canonical, namespaced target stabilized in
      [`cfc48132`](https://github.com/kcenon/network_system/commit/cfc48132fcfc206232deb3bf3001e66cd1b8b985)).
- [ ] Verify all includes are under `kcenon/network/` (no legacy
      `network_system::` namespace headers).
- [ ] If you reach into `cmake/compat/internal/...` shims, schedule the cutover
      to canonical `kcenon/network/internal/...` paths before v1.1.0.
- [ ] If you depend on the `KCENON_WITH_MONITORING_SYSTEM` macro, plan for
      v2.0.0+ removal.
- [ ] Continue using `kcenon::network::Result<T>` for error handling on the
      public API. Public functions either return `Result<T>` / `VoidResult` or
      are `noexcept`.

If your code already builds against `develop` as of 2026-05-10, no further
action is required for v1.0 itself.

## Table of Contents

- [What v1.0 Guarantees](#what-v10-guarantees)
- [Zero Breaking Changes](#zero-breaking-changes)
- [CMake Target Rename and Stabilization](#cmake-target-rename-and-stabilization)
- [Deprecated API Audit Summary](#deprecated-api-audit-summary)
- [Migration Steps for 0.x Consumers](#migration-steps-for-0x-consumers)
- [Build-System Migration](#build-system-migration)
- [Header-File Migration](#header-file-migration)
- [Code-Level Migration Patterns](#code-level-migration-patterns)
- [Breaking-Change Notice](#breaking-change-notice)
- [See Also](#see-also)

---

## What v1.0 Guarantees

v1.0 is the first SemVer-governed release of `network_system`. Once tagged,
the contract documented in
[`docs/v1.0-api-surface.md`](../v1.0-api-surface.md) becomes binding for the
entire v1.x cycle.

The guarantees, in order of strength:

1. **31 public headers are frozen.** Every header listed under the
   "Public API Surface" section of
   [`docs/v1.0-api-surface.md`](../v1.0-api-surface.md) is part of the v1.x
   SemVer contract. Symbols may not be removed, renamed, or have their
   signatures altered without a major version bump.
2. **`detail/` subtree (34 headers) remains internal.** These headers are
   implementation-detail and may evolve in any v1.x patch without SemVer
   impact. Do not include them directly.
3. **No `throw` in public headers.** Every public function either returns
   `kcenon::network::Result<T>` / `VoidResult` or is `noexcept`. This is
   enforced by the
   [`public-api-check`](../../.github/workflows/public-api-check.yml) CI gate
   on every PR.
4. **CMake export target is namespaced.** The canonical spelling is
   `network_system::network_system` for both build-tree (FetchContent /
   `add_subdirectory`) and install-tree (`find_package`) consumption.
5. **Adding new symbols and overloads is non-breaking.** v1.x minor releases
   may extend the surface but cannot remove from it.

For the per-header classification (12 functional groups, all 31 headers
classified as `stable`), see the "Per-Header Classification" section of
[`docs/v1.0-api-surface.md`](../v1.0-api-surface.md).

## Zero Breaking Changes

The v1.0 freeze was completed under
[#1139](https://github.com/kcenon/network_system/pull/1139)
(squash merge [`de159f34`](https://github.com/kcenon/network_system/commit/de159f34))
with **zero source-level removals**. The deprecated-API audit
([`docs/migration/deprecated_api_audit_v1_0.md`](deprecated_api_audit_v1_0.md))
explicitly documents this:

> The audit decision is to **freeze the deprecated surface as it stands** for
> v1.0:
> - A-tier: 1 retention (permanent — the macro IS the warning channel).
> - B-tier: 14 retentions, all targeted v1.1.0.
> - C-tier: 6 retentions, kept for binary compatibility through the v1.x line.

In practical terms:

- No public symbols were deleted.
- No public symbols were renamed.
- No public function signatures changed.
- No header files were removed.
- No header files were moved on the public surface.

If your 0.x code compiles and links against `develop` as of
[`de159f34`](https://github.com/kcenon/network_system/commit/de159f34),
it will compile and link against the v1.0 tag.

## CMake Target Rename and Stabilization

> **Stabilization commit**:
> [`cfc48132`](https://github.com/kcenon/network_system/commit/cfc48132fcfc206232deb3bf3001e66cd1b8b985)
> ([#1138](https://github.com/kcenon/network_system/pull/1138),
> closes [#1126](https://github.com/kcenon/network_system/issues/1126)).

The canonical CMake export target for v1.0 is **`network_system::network_system`**.

Prior to `cfc48132`, the install-tree spelling was implicit (driven only by
`NAMESPACE network_system::` in `cmake/network_system_install.cmake`). The
v1.0 stabilization pinned `EXPORT_NAME` on the `network_system` target so
the canonical namespaced spelling is locked across both consumption modes:

| Consumption mode | Canonical target |
|------------------|------------------|
| FetchContent / `add_subdirectory` (build-tree) | `network_system::network_system` (provided as an `ALIAS`) |
| `find_package(network_system CONFIG REQUIRED)` (install-tree) | `network_system::network_system` (exported via `EXPORT_NAME network_system` + `NAMESPACE network_system::`) |

### Verifying your CMake usage

Both of the following are correct and supported v1.0 usages:

```cmake
# Build-tree (FetchContent / add_subdirectory)
include(FetchContent)
FetchContent_Declare(
    network_system
    GIT_REPOSITORY https://github.com/kcenon/network_system.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(network_system)

target_link_libraries(your_target PRIVATE network_system::network_system)
```

```cmake
# Install-tree (after `cmake --install`)
find_package(network_system CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE network_system::network_system)
```

### What if my 0.x build used a different spelling?

If your 0.x build used the bare `network_system` target (without the namespace),
update it to `network_system::network_system`. The bare spelling was never
guaranteed and is not exported from the install-tree. The namespaced target is
the v1.0 stable contract and is verified by inspecting the generated
`network_system-targets.cmake` export file: only namespaced spellings are
emitted, and no deprecated targets exist.

## Deprecated API Audit Summary

The complete inventory lives in
[`deprecated_api_audit_v1_0.md`](deprecated_api_audit_v1_0.md). The audit
classified every deprecation marker into three tiers, each with a different
retention rationale and removal target.

### Tier A — Permanent retention (1 marker)

| ID  | Symbol | Removal target |
|-----|--------|----------------|
| A-1 | `NETWORK_EXPERIMENTAL_API` macro | None — permanent |

The macro is the documented mechanism for marking experimental protocols
(QUIC, reliable UDP). The `[[deprecated]]` attribute is repurposed to surface a
"use at your own risk" warning at every call site that opts in via
`NETWORK_USE_EXPERIMENTAL`. Removing it would silently weaken the
experimental-API contract.

**Migration impact:** none. The macro is internal to `src/internal/experimental/`
and is not on the v1.0 public surface.

### Tier B — v1.1.0 removal (14 shims)

The `cmake/compat/internal/...` directory contains 14 backward-compatibility
header shims that forward old include paths to the canonical
`<kcenon/network/internal/...>` locations. Each shim emits a deprecation
`#pragma message` at preprocessor time.

| Shim group | Old path prefix | Canonical replacement prefix | Count |
|------------|-----------------|------------------------------|-------|
| Experimental | `cmake/compat/internal/experimental/` | `kcenon/network/internal/experimental/` | 1 |
| Core | `cmake/compat/internal/core/` | `kcenon/network/internal/core/` | 5 |
| Integration | `cmake/compat/internal/integration/` | `kcenon/network/internal/integration/` | 6 |
| TCP | `cmake/compat/internal/tcp/` | `kcenon/network/internal/tcp/` | 1 |
| Interfaces | `cmake/compat/internal/interfaces/` | `kcenon/network/internal/interfaces/` | 1 |

**Removal target: v1.1.0.** All 14 shims are scheduled for removal in the next
minor release.

**Migration impact:** if your 0.x code includes any `cmake/compat/internal/...`
header, schedule the cutover to the canonical `kcenon/network/internal/...`
path before v1.1.0. The cutover is a one-line include-path change per file;
the symbols themselves are unchanged. See
[`deprecated_api_audit_v1_0.md`](deprecated_api_audit_v1_0.md) — section
"B. `#pragma message(\"Deprecated:\")` header-shim markers" — for the full
mapping table.

### Tier C — Long-lived deprecations (6 entries)

These items are documented as deprecated in `CHANGELOG.md` but do not carry a
`[[deprecated]]` source attribute. They remain supported through the entire
v1.x line.

| ID  | Symbol | Removal target |
|-----|--------|----------------|
| C-1 | `thread_system_pool_adapter` | Revisit post-v1.0; not before v3.0.0 |
| C-2 | `common_thread_pool_adapter` | Revisit post-v1.0; not before v3.0.0 |
| C-3 | `common_logger_adapter` | Revisit post-v1.0; not before v3.0.0 |
| C-4 | `common_monitoring_adapter` | Revisit post-v1.0; not before v3.0.0 |
| C-5 | `bind_thread_system_pool_into_manager()` | Revisit post-v1.0; not before v3.0.0 |
| C-6 | `KCENON_WITH_MONITORING_SYSTEM` macro | v2.0.0+ |

**Migration impact:** these are the legacy adapter classes superseded by
`NetworkSystemBridge`. If you are still on the adapter API, follow
[`adapter_to_bridge_migration.md`](adapter_to_bridge_migration.md) for a
guided migration. None of these will be removed during the v1.x cycle.

## Migration Steps for 0.x Consumers

The expected migration path for a typical 0.x consumer follows the steps
below. The complete frozen surface (31 public headers, all classified as
`stable`) is enumerated in
[`docs/v1.0-api-surface.md`](../v1.0-api-surface.md).

### Step 1 — Pin the version

Pin your dependency to `v1.0.0` (or the v1.x range you want to track).

```cmake
FetchContent_Declare(
    network_system
    GIT_REPOSITORY https://github.com/kcenon/network_system.git
    GIT_TAG v1.0.0
)
```

### Step 2 — Audit your CMake target spelling

Search your build system for any `network_system` references:

```bash
grep -rn 'network_system' CMakeLists.txt cmake/
```

For every `target_link_libraries(... network_system ...)` occurrence, ensure
the namespaced form is used:

```cmake
# Correct (v1.0 contract)
target_link_libraries(my_app PRIVATE network_system::network_system)
```

### Step 3 — Audit your include paths

Search for any non-canonical includes:

```bash
grep -rn '#include' src/ include/ \
  | grep -E 'network_system|kcenon/network|cmake/compat'
```

The canonical v1.0 include path is `<kcenon/network/...>`. Any include from
`cmake/compat/internal/...` should be migrated to the canonical
`<kcenon/network/internal/...>` path before v1.1.0.

### Step 4 — Confirm your error-handling contract

If your 0.x code catches exceptions from public `network_system` functions,
note that v1.0 public APIs do not throw. They return
`kcenon::network::Result<T>` or are `noexcept`. See the
[Code-Level Migration Patterns](#code-level-migration-patterns) section
below for examples.

### Step 5 — Build and run your existing test suite

The expected outcome is that no source changes are required. If you encounter
a build failure that does not trace to a CMake target spelling or
`cmake/compat/...` include, please open an issue against
`network_system` — that would be a v1.0 contract violation.

## Build-System Migration

### Implications of the EXPORT_NAME stabilization

The v1.0 stabilization commit
([`cfc48132`](https://github.com/kcenon/network_system/commit/cfc48132fcfc206232deb3bf3001e66cd1b8b985))
made the `EXPORT_NAME` explicit. The implications for downstream consumers:

1. **The install-tree spelling is now self-documenting.** The generated
   `network_system-targets.cmake` export file emits only the namespaced
   spelling.
2. **Internal target renames are now safe.** If `network_system` internally
   renames its target (e.g., for a refactoring), the install-tree contract
   stays at `network_system::network_system` because `EXPORT_NAME` decouples
   the install spelling from the source spelling.
3. **No deprecated target spellings are exported.** A reviewer-grep against
   `network_system-targets.cmake` after a clean install confirms only
   namespaced `network_system::` spellings.

### What to remove from your build files

- Any custom alias that wrapped the bare target (`network_system`).
- Any custom find-module that locates `network_system` outside of
  `find_package(network_system CONFIG REQUIRED)`. The canonical config-mode
  package is the supported v1.0 path.

### What to keep

- The standard `find_package(... CONFIG REQUIRED)` + `target_link_libraries`
  pair.
- Any optional dependency feature flags from
  [`config/feature_flags.h`](../../include/kcenon/network/config/feature_flags.h)
  (`KCENON_WITH_COMMON_SYSTEM`, `KCENON_WITH_THREAD_SYSTEM`,
  `KCENON_WITH_LOGGER_SYSTEM`, `KCENON_WITH_CONTAINER_SYSTEM`) — these are
  part of the v1.0 stable surface.

## Header-File Migration

The v1.0 API surface audit confirmed that **no header files were renamed,
moved, or removed on the public surface during the freeze window**. The 31
headers enumerated in the "Public API Surface" section of
[`docs/v1.0-api-surface.md`](../v1.0-api-surface.md) are at the same paths
they occupied in the most recent 0.x snapshot.

The only header-related migration applies to consumers reaching into
`cmake/compat/internal/...` shims (Tier B above). For those:

| Old include (Tier B compat shim) | Canonical v1.0 include | Removal of compat shim |
|----------------------------------|------------------------|------------------------|
| `<cmake/compat/internal/experimental/quic_client.h>` | `<kcenon/network/internal/experimental/quic_client.h>` | v1.1.0 |
| `<cmake/compat/internal/core/messaging_client.h>` | `<kcenon/network/internal/core/messaging_client.h>` | v1.1.0 |
| `<cmake/compat/internal/core/messaging_server.h>` | `<kcenon/network/internal/core/messaging_server.h>` | v1.1.0 |
| `<cmake/compat/internal/core/unified_messaging_client.h>` | `<kcenon/network/internal/core/unified_messaging_client.h>` | v1.1.0 |
| `<cmake/compat/internal/core/unified_messaging_server.h>` | `<kcenon/network/internal/core/unified_messaging_server.h>` | v1.1.0 |
| `<cmake/compat/internal/core/network_context.h>` | `<kcenon/network/internal/core/network_context.h>` | v1.1.0 |
| `<cmake/compat/internal/integration/monitoring_integration.h>` | `<kcenon/network/internal/integration/monitoring_integration.h>` | v1.1.0 |
| `<cmake/compat/internal/integration/logger_integration.h>` | `<kcenon/network/internal/integration/logger_integration.h>` | v1.1.0 |
| `<cmake/compat/internal/integration/messaging_bridge.h>` | `<kcenon/network/internal/integration/messaging_bridge.h>` | v1.1.0 |
| `<cmake/compat/internal/integration/container_integration.h>` | `<kcenon/network/internal/integration/container_integration.h>` | v1.1.0 |
| `<cmake/compat/internal/integration/thread_pool_bridge.h>` | `<kcenon/network/internal/integration/thread_pool_bridge.h>` | v1.1.0 |
| `<cmake/compat/internal/integration/bridge_interface.h>` | `<kcenon/network/internal/integration/bridge_interface.h>` | v1.1.0 |
| `<cmake/compat/internal/tcp/secure_tcp_socket.h>` | `<kcenon/network/internal/tcp/secure_tcp_socket.h>` | v1.1.0 |
| `<cmake/compat/internal/interfaces/i_quic_server.h>` | `<kcenon/network/internal/interfaces/i_quic_server.h>` | v1.1.0 |

**Recommended cutover policy:** complete the include-path migration during the
v1.0 adoption window so your build is shim-free before v1.1.0 lands.

> **Note:** consumers should prefer the public umbrella headers
> (`kcenon/network/network_system.h`, `facade/*.h`, `interfaces/*.h`) over
> any `internal/` path. The `internal/` headers are part of the implementation
> contract and may shift in v1.x patches; the public surface defined in
> `docs/v1.0-api-surface.md` is the SemVer-governed contract.

## Code-Level Migration Patterns

Because v1.0 introduces zero source-level breakages, most consumer code does
not change. The patterns below are reference snippets for the two situations
where a code change is recommended.

### Pattern 1 — Result<T> error handling (no exception catch)

Public `network_system` APIs do not throw. The CI gate
[`public-api-check.yml`](../../.github/workflows/public-api-check.yml) enforces
this, and the contract is documented in
[`docs/v1.0-api-surface.md`](../v1.0-api-surface.md) under
"Maintenance After Freeze".
If your 0.x code wraps a public call in `try`/`catch`, replace it with a
`Result<T>` check.

**Before (0.x exception-style — for any public API call):**

```cpp
#include <kcenon/network/network_system.h>

try {
    kcenon::network::initialize();
    // ...
} catch (const std::exception& e) {
    log_error("network_system init failed: {}", e.what());
    return -1;
}
```

**After (v1.0 Result<T>-style):**

```cpp
#include <kcenon/network/network_system.h>
#include <kcenon/network/types/result.h>

auto init_result = kcenon::network::initialize();
if (!init_result) {
    log_error("network_system init failed: {}",
              init_result.error().message());
    return -1;
}
```

Note that `Result<T>` semantics (`!result` on failure, `result.error()` for
the error info, `result.value()` for the success payload) are documented in
[`include/kcenon/network/types/result.h`](../../include/kcenon/network/types/result.h)
and align with `kcenon::common::Result<T>` when
`KCENON_WITH_COMMON_SYSTEM` is enabled.

### Pattern 2 — CMake target spelling

**Before (0.x — bare target spelling, never guaranteed):**

```cmake
target_link_libraries(my_app PRIVATE network_system)
```

**After (v1.0 — namespaced target, stable contract):**

```cmake
target_link_libraries(my_app PRIVATE network_system::network_system)
```

### Pattern 3 — Compat-shim include cutover (optional during v1.0, mandatory before v1.1.0)

**Before (Tier B compat shim, deprecated, removed in v1.1.0):**

```cpp
#include <cmake/compat/internal/core/messaging_client.h>
```

**After (canonical v1.0 path):**

```cpp
#include <kcenon/network/internal/core/messaging_client.h>
```

The symbols, declarations, and behavior are identical; only the include path
changes.

## Breaking-Change Notice

**v1.0 ships with zero breaking changes.**

This is a deliberate design choice recorded in
[`deprecated_api_audit_v1_0.md`](deprecated_api_audit_v1_0.md) under
"Removal-Targeted Symbols (this PR)":

> The audit decision is to **freeze the deprecated surface as it stands** for
> v1.0 [...] Per the issue's acceptance criteria, "removal-targeted symbols
> deleted in dedicated PRs" is satisfied vacuously — no symbols are targeted
> for removal in this audit.

The audit produced **0 removals** across the freeze window
(see PR [#1139](https://github.com/kcenon/network_system/pull/1139), squash
[`de159f34`](https://github.com/kcenon/network_system/commit/de159f34)).
Cross-checks:

- **Public API surface audit** ([`docs/v1.0-api-surface.md`](../v1.0-api-surface.md)):
  31 headers, all classified as `stable`. 0 rename candidates, 0 remove
  candidates, 0 experimental headers on the public surface.
- **Deprecated API audit** ([`deprecated_api_audit_v1_0.md`](deprecated_api_audit_v1_0.md)):
  21 deprecation markers found (1 A-tier, 14 B-tier, 6 C-tier). 0 removals,
  21 retentions.
- **Downstream consumer verification**
  (see the "Downstream Consumer Verification" section of
  [`docs/v1.0-api-surface.md`](../v1.0-api-surface.md)): the Tier 5 consumer
  [`pacs_system`](https://github.com/kcenon/pacs_system) builds against the
  frozen surface using only `facade/tcp_facade.h`, `interfaces/i_session.h`,
  and `session/session.h`. It does not reach into any `detail/`, deprecated,
  experimental, or rename-candidate symbol.

After v1.0, the next opportunity for source-level removals is **v1.1.0**, when
the 14 `cmake/compat/...` shims will be retired. A separate migration PR will
land before v1.1.0 to enumerate any additional retirements.

## See Also

- [`docs/v1.0-api-surface.md`](../v1.0-api-surface.md) — Public API surface
  audit (31 frozen headers, per-header classification, downstream consumer
  verification).
- [`docs/migration/deprecated_api_audit_v1_0.md`](deprecated_api_audit_v1_0.md) —
  Full deprecation inventory with disposition and removal targets.
- [`docs/migration/adapter_to_bridge_migration.md`](adapter_to_bridge_migration.md) —
  Migration from legacy adapter classes (Tier C symbols) to
  `NetworkSystemBridge`.
- [`docs/migration/network_system_bridge.md`](network_system_bridge.md) —
  `NetworkSystemBridge` facade reference.
- [`docs/CHANGELOG.md`](../CHANGELOG.md) — Canonical changelog (SSOT) — the
  v1.0 freeze entry under `[Unreleased]` documents the audit completion and
  links back to this guide.
- [`cmake/network_system_targets.cmake`](../../cmake/network_system_targets.cmake) —
  CMake target definition with the v1.0 `EXPORT_NAME` stabilization
  ([`cfc48132`](https://github.com/kcenon/network_system/commit/cfc48132fcfc206232deb3bf3001e66cd1b8b985)).
- [`.github/workflows/public-api-check.yml`](../../.github/workflows/public-api-check.yml) —
  CI gate enforcing "no `throw` in public headers".
- [#964](https://github.com/kcenon/network_system/issues/964) —
  Parent epic: prepare network_system for v1.0 release.
- [#1125](https://github.com/kcenon/network_system/issues/1125) —
  v1.0 API surface audit.
- [#1126](https://github.com/kcenon/network_system/issues/1126) —
  CMake export target stabilization.
- [#1127](https://github.com/kcenon/network_system/issues/1127) —
  Deprecated API audit for v1.0 freeze.
- [#1129](https://github.com/kcenon/network_system/issues/1129) —
  This migration guide.
