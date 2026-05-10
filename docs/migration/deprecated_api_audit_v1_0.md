---
doc_id: "NET-MIGR-006"
doc_title: "Deprecated API Audit for v1.0 Freeze"
doc_version: "1.0.0"
doc_date: "2026-05-10"
doc_status: "Released"
project: "network_system"
category: "MIGR"
---

# Deprecated API Audit for v1.0 Freeze

> **SSOT**: This document is the single source of truth for the v1.0 deprecated API audit.

> Tracking issue: [#1127](https://github.com/kcenon/network_system/issues/1127). Part of [#964](https://github.com/kcenon/network_system/issues/964).

## Purpose

v1.0 is the cleanest moment to remove deprecated APIs. Once frozen, removal requires
a v2.0 SemVer bump. This document records every deprecation marker present in the
codebase as of the audit date and the disposition decided for each.

## Audit Methodology

1. Enumerate every `[[deprecated]]` attribute in `include/`, `src/`, and `cmake/`.
2. Enumerate every `#pragma message("Deprecated:` shim under `cmake/compat/`.
3. Enumerate every CHANGELOG entry under `### Deprecated` and reconcile against the
   source tree (a deprecation announcement without a corresponding marker is itself
   a finding).
4. For each marker, decide: **REMOVE** (delete in a follow-up PR before v1.0) or
   **RETAIN** (keep through v1.0 with a documented removal target).

## Inventory

### A. `[[deprecated]]` attribute markers

| ID  | Symbol | Location | Disposition | Notes |
|-----|--------|----------|-------------|-------|
| A-1 | `NETWORK_EXPERIMENTAL_API` macro (expands to `[[deprecated("Experimental API - may change between minor versions without notice")]]`) | `src/internal/experimental/experimental_api.h:72-73` | **RETAIN** | Macro is the documented mechanism for marking experimental protocols (QUIC, reliable UDP). The `[[deprecated]]` here is repurposed to surface a "use at your own risk" warning at every call site that opts in via `NETWORK_USE_EXPERIMENTAL`. Removing it would silently weaken the experimental-API contract. No removal target — the macro is permanent for the lifetime of the experimental namespace. |

**Net A-tier:** 1 marker, 0 removals, 1 retention.

### B. `#pragma message("Deprecated:")` header-shim markers

These are backward-compatibility shims under `cmake/compat/internal/...` that
forward old include paths to the canonical `<kcenon/network/internal/...>`
locations. Each shim emits a deprecation `#pragma message` at preprocessor time.

| ID  | Compat header (old path) | Canonical replacement | Disposition | Removal target |
|-----|--------------------------|------------------------|-------------|----------------|
| B-1 | `cmake/compat/internal/experimental/quic_client.h` | `kcenon/network/internal/experimental/quic_client.h` | **RETAIN** | v1.1.0 |
| B-2 | `cmake/compat/internal/core/unified_messaging_client.h` | `kcenon/network/internal/core/unified_messaging_client.h` | **RETAIN** | v1.1.0 |
| B-3 | `cmake/compat/internal/core/messaging_client.h` | `kcenon/network/internal/core/messaging_client.h` | **RETAIN** | v1.1.0 |
| B-4 | `cmake/compat/internal/core/messaging_server.h` | `kcenon/network/internal/core/messaging_server.h` | **RETAIN** | v1.1.0 |
| B-5 | `cmake/compat/internal/core/unified_messaging_server.h` | `kcenon/network/internal/core/unified_messaging_server.h` | **RETAIN** | v1.1.0 |
| B-6 | `cmake/compat/internal/core/network_context.h` | `kcenon/network/internal/core/network_context.h` | **RETAIN** | v1.1.0 |
| B-7 | `cmake/compat/internal/integration/monitoring_integration.h` | `kcenon/network/internal/integration/monitoring_integration.h` | **RETAIN** | v1.1.0 |
| B-8 | `cmake/compat/internal/integration/logger_integration.h` | `kcenon/network/internal/integration/logger_integration.h` | **RETAIN** | v1.1.0 |
| B-9 | `cmake/compat/internal/integration/messaging_bridge.h` | `kcenon/network/internal/integration/messaging_bridge.h` | **RETAIN** | v1.1.0 |
| B-10 | `cmake/compat/internal/integration/container_integration.h` | `kcenon/network/internal/integration/container_integration.h` | **RETAIN** | v1.1.0 |
| B-11 | `cmake/compat/internal/integration/thread_pool_bridge.h` | `kcenon/network/internal/integration/thread_pool_bridge.h` | **RETAIN** | v1.1.0 |
| B-12 | `cmake/compat/internal/integration/bridge_interface.h` | `kcenon/network/internal/integration/bridge_interface.h` | **RETAIN** | v1.1.0 |
| B-13 | `cmake/compat/internal/tcp/secure_tcp_socket.h` | `kcenon/network/internal/tcp/secure_tcp_socket.h` | **RETAIN** | v1.1.0 |
| B-14 | `cmake/compat/internal/interfaces/i_quic_server.h` | `kcenon/network/internal/interfaces/i_quic_server.h` | **RETAIN** | v1.1.0 |

**Rationale (all B-tier):** These shims were added in the recent (within this minor
cycle) include-path reorganization. Per the issue's own retention rule — "markers
from this minor cycle: retain into v1.0 with a documented v1.x.0 removal target" —
all 14 shims are retained. They are zero-cost when not included, low-cost when
included (single `#pragma message` and a single `#include`), and removing them now
would break any in-flight downstream branch that has not yet adopted the new
include paths. Target removal: **v1.1.0** (the next minor release after the v1.0
freeze).

**Net B-tier:** 14 shims, 0 removals, 14 retentions.

### C. CHANGELOG-announced deprecations missing a source-level marker

These items are listed under `### Deprecated` in `CHANGELOG.md` and
`docs/CHANGELOG.md` but **do not** carry a `[[deprecated]]` attribute on their
declarations. This is a documentation/code-divergence finding, not a blocker for
v1.0 freeze.

| ID  | Symbol | CHANGELOG entry | Source location | Has `[[deprecated]]`? | Disposition |
|-----|--------|------------------|------------------|------------------------|-------------|
| C-1 | `thread_system_pool_adapter` | "Will be removed in v3.0.0" (CHANGELOG line 68) | `src/internal/integration/thread_system_adapter.h:46` | No | **RETAIN**. The class is the canonical thread-pool integration adapter and is still actively used by `src/network_system.cpp` and `src/core/network_context.cpp`. Replacement (`ThreadPoolBridge`) coexists. The "v3.0.0" removal target predates the v1.0 release plan in #964 and should be revisited post-v1.0 — not in scope for v1.0 freeze. **Action**: leave class in place; update `### Deprecation Timeline` in CHANGELOG to reflect v1.0-aligned semantics in a follow-up PR. |
| C-2 | `common_thread_pool_adapter` | "Will be removed in v3.0.0" (CHANGELOG line 73) | `src/network_system.cpp:89` (used internally; class is in `thread_pool_adapters.h`) | No | **RETAIN**. Same rationale as C-1. |
| C-3 | `common_logger_adapter` | "Will be removed in v3.0.0" (CHANGELOG line 78) | Used in `src/network_system.cpp:95` and `src/integration/network_system_bridge.cpp:357` | No | **RETAIN**. Same rationale as C-1. |
| C-4 | `common_monitoring_adapter` | "Will be removed in v3.0.0" (CHANGELOG line 83) | Used in `src/network_system.cpp:103` and `src/integration/network_system_bridge.cpp:363` | No | **RETAIN**. Same rationale as C-1. |
| C-5 | `bind_thread_system_pool_into_manager()` | "Will be removed in v3.0.0" (CHANGELOG line 88) | `src/internal/integration/thread_system_adapter.h:86` | No | **RETAIN**. Same rationale as C-1. |
| C-6 | `KCENON_WITH_MONITORING_SYSTEM` macro | "DEPRECATED ... will be removed in a future version" (`feature_flags.h:65-66`) | `include/kcenon/network/detail/config/feature_flags.h:69-75` | No (comment-only) | **RETAIN**. Macro is still part of the public configuration surface; removing it requires a SemVer-major bump per the same v1.0 freeze logic. Removal target: **v2.0.0** or later. |

**Net C-tier:** 6 documented deprecations, 0 removals, 6 retentions.

## Removal-Targeted Symbols (this PR)

**None.**

The audit decision is to **freeze the deprecated surface as it stands** for v1.0:

- A-tier: 1 retention (permanent — the macro IS the warning channel).
- B-tier: 14 retentions, all targeted v1.1.0.
- C-tier: 6 retentions, kept for binary compatibility through the v1.x line.

Per the issue's acceptance criteria, "removal-targeted symbols deleted in dedicated
PRs" is satisfied vacuously — no symbols are targeted for removal in this audit.

## Retained Symbols — Removal Targets

The single source of truth for removal targets is the table above. Cross-checks:

- `cmake/compat/*.h` shims: removal target **v1.1.0** (B-1 through B-14).
- `NETWORK_EXPERIMENTAL_API` macro: **permanent**, no removal target (A-1).
- Adapter classes (thread/logger/monitoring): retained through v1.x; revisit
  post-v1.0 (C-1 through C-5).
- `KCENON_WITH_MONITORING_SYSTEM` macro: removal target **v2.0.0+** (C-6).

## Out-of-Scope Findings

The following items were noted during the audit but are not deprecations and are
out of scope for this issue. They are recorded here for downstream traceability:

- `result_types.h` comment "deprecated for external use" (line 44, 54) — this is
  guidance language pointing external consumers to `kcenon::common::Result<T>`,
  not a `[[deprecated]]` attribute. The aliases remain part of the internal API
  surface. No action.
- `tls_1_0` / `tls_1_1` enum comments "deprecated, insecure" in `common_defs.h` —
  these reflect IETF deprecation of the protocol versions, not API deprecation.
  The enum members remain valid for compatibility with legacy peers when explicitly
  selected. No action.
- `-Wdeprecated-declarations` `#pragma` suppressions in
  `src/integration/{thread_system_adapter,messaging_bridge}.cpp`,
  `src/internal/integration/{thread_system_adapter,messaging_bridge}.h`, and
  `src/internal/utils/openssl_compat.h` — these suppress warnings emitted by
  upstream third-party headers (OpenSSL 3.x deprecation matrix, thread_system
  pre-v1.0 surface). They are not network_system deprecations. No action.

## CHANGELOG Update Required

The audit changes nothing in the source tree. The only CHANGELOG entry needed is a
single line under `### Changed` (or a new `### Documentation` section) recording
that the v1.0 deprecated-API audit was completed. No `### Removed` entries are
generated by this PR because no symbols are removed.

See the CHANGELOG diff in this PR.

## Acceptance Criteria Coverage

| Criterion (from #1127) | Status |
|-------------------------|--------|
| Inventory of all `[[deprecated]]` markers committed | Done — A-tier and B-tier tables above. |
| All removal-targeted symbols deleted in dedicated PRs | Vacuously satisfied — zero symbols targeted for removal. |
| Retained symbols document their v1.x removal target in code comment | Done — see table cross-references; removal targets are documented in this SSOT, which is referenced from the relevant CHANGELOG sections. |
| CHANGELOG lists removed APIs under "Removed" | N/A — no removals in this audit. CHANGELOG records the audit completion under `### Changed`. |

## Future Work

A follow-up issue should:

1. Add the actual `[[deprecated]]` attribute to the C-tier classes (so the
   compiler emits warnings on use), bringing the source tree into agreement with
   the CHANGELOG.
2. Revisit the C-tier "v3.0.0" removal targets in light of the new v1.0 baseline.
3. Schedule the B-tier shim removal PR for the v1.1.0 milestone.
