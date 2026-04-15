# Document Review Report — network_system

**Generated**: 2026-04-14
**Mode**: Report-only
**Files analyzed**: 117 (excluding `build/`, `.git/`)

## Findings Summary

| Severity     | Phase 1 | Phase 2 | Phase 3 | Total |
|--------------|---------|---------|---------|-------|
| Must-Fix     |     126 |      16 |       6 |   148 |
| Should-Fix   |       2 |      13 |      12 |    27 |
| Nice-to-Have |       0 |       4 |       8 |    12 |
| **Total**    |   **128** |  **33** |  **26** | **187** |

Totals by phase (all severities): Phase 1 = 128, Phase 2 = 33, Phase 3 = 26.

Global stats:
- Total anchors: 3,393
- Total links: 1,189
- Broken links/anchors: 126 (Phase 1 Must-Fix), broken link rate ~10.6%
- Unbalanced code fences: 2 files

---

## Must-Fix Items

### Phase 1 — Anchors & File Links (126 items)

Broken inter-file links (91): target file does not exist at the specified relative path.
Broken intra-file anchors (35): anchor referenced but matching heading missing.

Most frequent broken targets (suggests systematic issues):

| Count | Target                                 | Likely Root Cause |
|-------|----------------------------------------|--------------------------|
|   7x  | `TLS_SETUP_GUIDE.md`                   | References use bare filename; file lives in `docs/guides/` |
|   7x  | `TROUBLESHOOTING.md`                   | Same as above (file is in `docs/guides/`) |
|   4x  | `API_REFERENCE.md`                     | References use bare name; file is in `docs/` |
|   3x  | `../../../ECOSYSTEM.md`                | Wrong relative depth from `docs/integration/` and `docs/integration/with-*.md` |
|   2x  | `../IMPROVEMENTS.md`                   | No such file anywhere in repo |
|   2x  | `docs/`                                | Empty link stub in CHANGELOG |
|   2x  | `BUILD.md`                             | File is `docs/guides/BUILD.md` |
|   2x  | `../UDP_SUPPORT.md`                    | File is `docs/guides/UDP_SUPPORT.md` |
|   2x  | `with-thread-system.md`                | Not yet written (referenced as planned) |
|   2x  | `with-database-system.md`              | Not yet written (referenced as planned) |

Phase 1 broken-link samples (first 30; full list below):

1. `CONTRIBUTING.md:64` — broken file link `../ARCHITECTURE.md` (root-level CONTRIBUTING uses wrong path; should be `docs/ARCHITECTURE.md`).
2. `CONTRIBUTING.md:65` — broken file link `../API_REFERENCE.md`.
3. `CONTRIBUTING.md:66` — broken file link `../guides/BUILD.md`.
4. `README.md:692` — broken file link `docs/BUILD.md` (actual: `docs/guides/BUILD.md`).
5. `README.md:693` — broken file link `docs/MIGRATION_GUIDE.md` (no such file; `docs/MIGRATION.md` or `docs/refactoring/MIGRATION_GUIDE_V2.md`).
6. `README.md:700-707` — `docs/TLS_SETUP_GUIDE.md`, `docs/TROUBLESHOOTING.md`, `docs/LOAD_TEST_GUIDE.md`, `docs/OPERATIONS.md` all missing (files exist under `docs/guides/` or `docs/advanced/`).
7. `README.md:908` — `IMPROVEMENTS.md` does not exist.
8. `README.md:918` — `CODING_STYLE_RULES.md` does not exist.
9. `docs/ARCHITECTURE_OVERVIEW.md:38` — intra-file anchor `#purpose--design-goals` missing (heading uses `&` differently; likely `#purpose-design-goals`).
10. `docs/ARCHITECTURE_OVERVIEW.md:687` — broken `../INTEGRATION.md` (should be `./INTEGRATION.md` relative to `docs/`).
11. `docs/BENCHMARKS.md:530` — `LOAD_TEST_GUIDE.md` (file is in `docs/guides/`).
12. `docs/BENCHMARKS.md:533` — `../IMPROVEMENTS.md` no such file.
13. `docs/CHANGELOG.kr.md:824`, `docs/CHANGELOG.md:1019` — link target `docs/` is empty.
14. `docs/DESIGN_DECISIONS.md:15` — `DESIGN_DECISIONS.kr.md` does not exist (English→Korean link placeholder).
15. `docs/FEATURES.md:128,984,985` — `TLS_SETUP_GUIDE.md`, `MIGRATION_GUIDE.md` bare names.
16. `docs/FEATURES.md:930` — `../IMPROVEMENTS.md` no such file.
17. `docs/PRODUCTION_QUALITY.md:302,781` and `docs/PRODUCTION_QUALITY.kr.md:169,416,419` — `TLS_SETUP_GUIDE.md`, `TLS_SETUP_GUIDE.kr.md`, `TROUBLESHOOTING*.md` (need `../guides/` prefix).
18. `docs/PROJECT_STRUCTURE.md:619`, `docs/PROJECT_STRUCTURE.kr.md:561` — `BUILD.md`.
19. `docs/README.kr.md:271` — `../../docs/ECOSYSTEM_OVERVIEW.kr.md` does not exist; `docs/ECOSYSTEM.md` exists.
20. `docs/UNIFIED_API_GUIDE.md:23,24,25` — intra anchors `#i_transport-interface`, `#i_connection-interface`, `#i_listener-interface` missing (heading slugs mismatch due to escaped underscores).
21. `docs/advanced/CONCEPTS.md:36` — intra anchor `#integration-with-common_system` missing.
22. `docs/advanced/CONNECTION_POOLING.md:454,456` — `API_REFERENCE.md`, `TROUBLESHOOTING.md` bare.
23. `docs/advanced/HTTP_ADVANCED.md:513-515` — `API_REFERENCE.md`, `TLS_SETUP_GUIDE.md`, `TROUBLESHOOTING.md`.
24. `docs/advanced/MIGRATION.md:15` — `MIGRATION_KO.md` does not exist (convention elsewhere is `MIGRATION.kr.md`).
25. `docs/advanced/MIGRATION.md:1477-1491, 1539` — **11 absolute paths** starting with `/Users/raphaelshin/Sources/network_system/...` (committed dev-machine paths; broken anywhere else).
26. `docs/advanced/OPERATIONS.md:806` / `docs/advanced/OPERATIONS.kr.md:806` — `TROUBLESHOOTING.md` / `TROUBLESHOOTING.kr.md` bare (need `../guides/`).
27. `docs/advanced/PERFORMANCE_TUNING.md:815,818` — `BENCHMARKS.md`, `TROUBLESHOOTING.md` bare.
28. `docs/advanced/UDP_RELIABILITY.md:94,534,535,537` — `../UDP_SUPPORT.md` (file is `docs/guides/UDP_SUPPORT.md`), `API_REFERENCE.md`, `TROUBLESHOOTING.md`.
29. `docs/guides/GRPC_GUIDE.md:15` — `GRPC_GUIDE.kr.md` does not exist.
30. `docs/guides/LOAD_TEST_GUIDE.md:424` — `WEBSOCKET_IMPLEMENTATION_PLAN.md` does not exist.
31. `docs/guides/MIGRATION_UNIFIED_API.md:387` — `../../examples/unified/` not present.
32. `docs/guides/TROUBLESHOOTING.md:1091,1092` / `docs/guides/TROUBLESHOOTING.kr.md:1091,1092` — `OPERATIONS.md`, `API_REFERENCE.md` bare (files are in `../advanced/` and `..`).
33. `docs/implementation/01-architecture-and-components.md:17` — `01-architecture-and-components.kr.md` does not exist.
34. `docs/implementation/01-architecture-and-components.md:30-63` — **34 intra-file anchors missing**. TOC references sections (Dependency Management, Test Framework, Performance Monitoring, Resource Management Patterns, Error Handling Strategy, sub-sections for Smart Pointer Strategy, Thread Safety, etc.) that were **never written**. File ends after "Session Management" at line 525; TOC promises content through line 63.
35. `docs/implementation/02-dependency-and-testing.md:17`, `03-performance-and-resources.md:17`, `04-error-handling.md:17`, `README.md:15` — all reference missing `*.kr.md` counterparts.
36. `docs/implementation/README.md:65,67,70,74,78` — anchors promised in `01-*.md`, `02-*.md`, `03-*.md` do not exist (symptomatic of #34 above).
37. `docs/implementation/README.md:88` — `../MIGRATION_GUIDE.md` does not exist.
38. `docs/integration/README.md:24,25,148,154` — `with-thread-system.md`, `with-database-system.md` referenced but not written.
39. `docs/integration/README.md:163,172,178,179` — `../performance/OPTIMIZATION.md`, `../security/SECURITY.md`, `../../../ECOSYSTEM.md`, `../../../examples/network/` all missing.
40. `docs/integration/with-common-system.md:558-560` — references `../../../common_system/...` (external repo assumed alongside).
41. `docs/integration/with-logger.md:334-336,342,343` — `../../../examples/tcp_server_with_logging/`, `../../../examples/http_client_with_logging/`, `../../../examples/websocket_logging/`, `../guides/LOGGING.md`, `../../../ECOSYSTEM.md` — first four non-existent; last is wrong depth.
42. `docs/migration/adapter_to_bridge_migration.md:460,461`, `docs/migration/network_system_bridge.md:430,431,432` — `../api/*.md`, `../architecture/*.md` directories and files do not exist.
43. `docs/guides/TLS_SETUP_GUIDE.md:20,25,26,38,39,40` + `.kr.md` variants — 12 intra-file anchors missing because TOC uses emoji-prefixed headings; the TOC links predict a GitHub slug that doesn't match how the author wrote the target heading (e.g., `#-current-status` expected, but heading is `⚠️ Current Status` which GitHub actually slugs as `-current-status`; the real breakage is that there's NO such heading in the file at all — only the TOC entries remain).

Full Phase 1 broken list: see machine-readable breakdown in the "Notes" section.

### Phase 2 — Accuracy (16 Must-Fix)

1. **Committed absolute paths** — `docs/advanced/MIGRATION.md:1477-1491, 1539` contains 11 links with `/Users/raphaelshin/Sources/network_system/...`. These break on every other machine and leak the original author's username. Must be converted to relative paths.
2. **Version drift (project version)** — `CMakeLists.txt` declares `VERSION 0.1.1`, but `docs/CHANGELOG.md:635` lists `[1.5.0] - 2025-11-14` as latest release with `[1.4.0]` marked "Current" in the version table (`docs/CHANGELOG.md:1029`). These cannot both be true.
3. **CMake minimum version inconsistency** — Actual `cmake_minimum_required(VERSION 3.20)` (top-level `CMakeLists.txt:1`). Docs claim:
   - `docs/guides/BUILD.md:31`: "CMake 3.16 or later"
   - `docs/guides/BUILD.kr.md:31`: "CMake 3.16 이상"
   - `docs/contributing/CONTRIBUTING.md:59`: "CMake 3.16 or higher"
   - `docs/guides/LOAD_TEST_GUIDE.md:31`: "CMake 3.16 or higher"
   - `docs/PROJECT_STRUCTURE.md:454`, `docs/PROJECT_STRUCTURE.kr.md:409`: "CMake 3.16+"
   - `docs/FEATURES.md:924`: "CMake 3.16+"
   - `docs/advanced/MIGRATION.md:235,257,614,626`: `cmake_minimum_required(VERSION 3.16)` in examples
   - Only `docs/guides/QUICK_START.md:23,174` and `.kr.md` use CMake 3.20 (correct). `CHANGELOG.md:977` historically mentions 3.16, which is fine as history.
4. **Compiler requirement inconsistency** — Primary build only requires C++20 (no explicit compiler check). Modules build requires Clang 16+ / GCC 14+ / MSVC 17.4+ (`CMakeLists.txt:60`). Docs claim incompatible minimums:
   - `README.md:105,716-720,769`: GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+.
   - `README.kr.md:282-284`: GCC 13+, Clang 17+.
   - `docs/README.kr.md:230-232`: GCC 9+, Clang 10+ (macOS 12+, Apple Clang 14+, Windows 11 MSVC 2019+) — **much lower than English README**.
   - `docs/PROJECT_STRUCTURE.md:453`, `docs/guides/QUICK_START.md:24`, `docs/PRODUCTION_QUALITY.md:89`: GCC 11+, Clang 14+.
   - `docs/FEATURES.md:918,922`: GCC 11+, Apple Clang 14+.
   - `docs/guides/LOAD_TEST_GUIDE.md:32`: GCC 11+, Clang 13+, MSVC 2019+.
   - `docs/contributing/CONTRIBUTING.md:56,57`: Clang 12+, GCC 10+.
   - `docs/advanced/MIGRATION.md:199,894`: GCC 10+, Clang 10+, "Install GCC 11+".
   - `samples/README.md:102`, `samples/README_KO.md:102`: "GCC 10+, Clang 12+, MSVC 2019+".
   - `CLAUDE.md:108`: "GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+".
   At least 5 distinct compiler-version tiers across docs — must be reconciled to a single source.
5. **ASIO minimum version inconsistency**:
   - `CLAUDE.md:101`: "ASIO >= 1.30.2".
   - `CONTRIBUTING.md:48`, `docs/contributing/CONTRIBUTING.md:62`, `docs/PROJECT_STRUCTURE.md:455`, `docs/PROJECT_STRUCTURE.kr.md:410`, `docs/guides/BUILD.md:32`, `docs/guides/BUILD.kr.md:32`, `.github/workflows/release.yml:207`: "ASIO 1.28+" / "Boost.ASIO 1.28+".
   - CI/build workflows actually install ASIO 1.32.0 from source.
   - `CMakeLists.txt:221-225`: notes that ASIO 1.31.0+ has native fixes.
   Correct ASIO minimum should be stated consistently (1.28+ or 1.30.2+ — pick one).
6. **Boost.ASIO vs standalone ASIO contradiction**:
   - `cmake/network_system_integration.cmake:12-13`: "**This project only supports standalone ASIO (not Boost.ASIO)**".
   - `cmake/network_system_dependencies.cmake:130`: "we skip Boost.ASIO and fetch standalone ASIO".
   - `.github/workflows/release.yml:207`: "Standalone ASIO 1.28+ (Boost.ASIO not supported)".
   - But the following docs still allow Boost.ASIO:
     - `CONTRIBUTING.md:48`, `docs/contributing/CONTRIBUTING.md:62`: "ASIO or Boost.ASIO 1.28+".
     - `docs/guides/BUILD.md:32`, `docs/guides/BUILD.kr.md:32`: "ASIO library or Boost.ASIO 1.28+".
     - `docs/PROJECT_STRUCTURE.md:455`, `docs/PROJECT_STRUCTURE.kr.md:410`: "ASIO 1.28+ (or Boost.ASIO)".
     - `docs/ARCHITECTURE_OVERVIEW.md:241`: "Boost.ASIO or standalone ASIO".
     - `docs/advanced/MIGRATION.md:919,920`: "Or use Boost.Asio instead ... network_system will automatically detect and use Boost.Asio".
     - `docs/BENCHMARKS.md:332`: "| Boost.Asio | TBD |" (unsubstantiated benchmark row).
   This is a **factual contradiction** with build requirements.
7. **Implementation file left incomplete** — `docs/implementation/01-architecture-and-components.md` TOC (lines 23-63) promises sections that do not exist in the file. Either the TOC is wrong or the file is unfinished. (Links to 34 missing anchors already counted in Phase 1.)
8. **Year-stamped dates need review** —
   - `docs/integration/with-monitoring.md:315`: "*Last Updated: 2025-01-26*" (over a year old; file may or may not be current as of 2026-04-14).
   - Most other `Last Updated` dates are 2025 (oldest 2025-09-19, newest 2025-12-27). As today is 2026-04-14, all are at least 4 months stale; documents declaring `doc_date: "2026-04-04"` in frontmatter appear to be the intended current-date stamp — verify whether content was actually reviewed on that date.

### Phase 3 — SSOT & Redundancy (6 Must-Fix)

1. **Duplicate SSOT claims for the same topic**:
   - "Integration Guide" is claimed by THREE documents simultaneously:
     - `docs/INTEGRATION.md:13` — "SSOT for **Integration Guide**".
     - `docs/INTEGRATION.kr.md:13` — "SSOT for **Integration Guide**" (same English title, not "통합 가이드").
     - `docs/integration/README.md:13` — "SSOT for **Network System Integration Guide**".
   Three docs cannot all be the single source of truth for the same English topic. The `.kr.md` should say "SSOT for **통합 가이드**" and the integration/README.md should be subordinate.
2. **Migration SSOT fragmentation** — Seven separate documents each claim SSOT for migration topics:
   - `docs/MIGRATION.md` — "Monolithic to Modular Architecture"
   - `docs/advanced/MIGRATION.md` — "messaging_system to network_system"
   - `docs/refactoring/MIGRATION_GUIDE_V2.md` — "network_system v1.x → v2.0"
   - `docs/guides/MIGRATION_UNIFIED_API.md` — "Unified Interface API"
   - `docs/migration/adapter_to_bridge_migration.md` — "From Adapters to NetworkSystemBridge"
   - `docs/migration/network_system_bridge.md` — "NetworkSystemBridge Migration"
   - `docs/facades/migration-guide.md` — "Facade Pattern Migration"
   Overlap between "Monolithic to Modular" and "v1.x → v2.0" is especially significant (same semantic transition). Needs a single migration index and non-overlapping scoped sub-docs.
3. **Duplicate `network-core/README.md`** — `network-core/README.md` and `libs/network-core/README.md` both exist and **differ in content** (e.g., `network-core/README.md` line 3 says "Core interfaces and abstractions for the network_system protocol libraries"; `libs/network-core/README.md` line 3 says "Core interfaces and abstractions for the network_system modularization effort"). One must be removed or made a link.
4. **Architecture split inconsistency** — `docs/ARCHITECTURE.md` (index) correctly delegates to `ARCHITECTURE_OVERVIEW.md` and `ARCHITECTURE_PROTOCOLS.md`. However `docs/ARCHITECTURE_OVERVIEW.md:687` links back to `../INTEGRATION.md` (broken) — the cross-ref inside the split is itself broken.
5. **API Reference split inconsistency** — `docs/API_REFERENCE.md` is a 43-line index; `API_REFERENCE_FACADES.md` (1262 lines) and `API_REFERENCE_PROTOCOLS.md` (589 lines) are the split halves. `API_REFERENCE.kr.md` (1483 lines) is a single-file Korean version not mirrored to the split, creating an EN/KR structural mismatch.
6. **`CHANGELOG.md` at root vs `docs/CHANGELOG.md`** — Both exist (`/CHANGELOG.md` and `docs/CHANGELOG.md`). The latter declares SSOT for changelog. The root-level file should either be a symlink, a pointer stub, or removed. Current state risks divergence.

---

## Should-Fix Items

### Phase 1 (2)

1. **Unbalanced fenced code blocks** in:
   - `docs/API_REFERENCE_FACADES.md` — 59 fence lines (odd count). Stray bare ```` ``` ```` at lines 344 and 676 create two "extra" fences, causing markdown renderers to misinterpret sections after them. Visible symptom: the file parses with `in_code=True` flipped in the middle of normal prose.
   - `docs/API_REFERENCE_PROTOCOLS.md` — 21 fence lines (odd count).
2. **Slug collisions not deduped by authors** — Several TOCs use `1. Something` patterns that normally slugify to `1-something`; confirm duplicate section titles do not rely on the `-1`, `-2` duplicate-suffix convention. Review: `docs/API_REFERENCE_FACADES.md` TOC has "Examples" that appears multiple times.

### Phase 2 (13)

1. **Terminology — "Boost.Asio" vs "Boost.ASIO"** mixed throughout (`docs/BENCHMARKS.md` uses `Boost.Asio`, most other docs use `Boost.ASIO`). Even though project doesn't support Boost.ASIO, comparative references should be consistent.
2. **"Asio"/"ASIO"/"asio"** — used inconsistently. The upstream project uses "Asio" (lowercase after A) in docs. Repo-wide: README.md mixes `ASIO 1.28+` with `std::asio`; recommend standardizing to "Asio" in prose and `asio::` in code.
3. **`session` / `Session` / `messaging_session`** — inconsistent capitalization when describing the concept. In `docs/ARCHITECTURE_OVERVIEW.md`, `docs/ARCHITECTURE_PROTOCOLS.md`, `docs/advanced/MIGRATION.md` — use "session" for concept, `Session` for class, `messaging_session` for the concrete C++ type.
4. **`handler` disambiguation** — `handler` is used both for "callback" and "event_handler interface" without explicit distinction; readers of `docs/UNIFIED_API_GUIDE.md` and `docs/ARCHITECTURE_OVERVIEW.md` will conflate them.
5. **"transport" vs "protocol"** — `docs/UNIFIED_API_GUIDE.md` uses `i_transport` interface; `docs/ARCHITECTURE_PROTOCOLS.md` uses "protocol components". Pick one noun.
6. **Coroutine status description** — `docs/FEATURES.md:663` says "C++20 coroutine infrastructure is in place; more awaitable types are planned" while `docs/README.kr.md:225` says "C++20 코루틴 지원" (present) without caveat. Should be consistent.
7. **OpenSSL version floor** — `CONTRIBUTING.md:49` says "OpenSSL 3.0+", `CMakeLists.txt:601` says `find_package(OpenSSL 3.0.0 REQUIRED)`. Consistent. BUT `docs/protocols/quic/CONFIGURATION.md:35` and `docs/protocols/quic/README.md:119` say "OpenSSL 3.0+" — fine. OK for P2 but the README.md:114-116 warning language is softer ("strongly recommend upgrading") than the CMake hard-fail. Reconcile tone.
8. **"Asio version" prose inconsistency** — Some docs write "ASIO 1.28+", some "ASIO 1.28.1", some "1.30.2", some "1.32.0". Not all are wrong, but readers cannot tell which is minimum vs installed.
9. **External URL validity** — Not exhaustively checked in this review. Spot checks of GitHub references are stable; no known dead external links were flagged.
10. **Year references** — No future-year issues found relative to today (2026-04-14). All timestamped content is 2024-2026.
11. **`GCC 9+`, `Clang 10+` in `docs/README.kr.md:230`** — significantly lower than English README's `GCC 13+`; Korean doc is wrong.
12. **`docs/CHANGELOG.md:1029-1034` table** marks "1.4.0" as "Current", but `docs/CHANGELOG.md:635` shows "1.5.0 - 2025-11-14" as a newer release. Status table not updated when 1.5.0 was added.
13. **`docs/design/composition-design.md:17` / `crtp-analysis.md:17`** use "Last Updated: 2025-01-13" / "2025-01-15" — well before the bulk of docs (2025-10 to 2025-12). Verify still applicable.

### Phase 3 (12)

1. **Missing bidirectional cross-refs** — `docs/DESIGN_DECISIONS.md` links to ADR files but none of the three ADRs link back to `DESIGN_DECISIONS.md`. Add a "See also" footer in each ADR.
2. **`docs/README.md` (index)** — does not link to `docs/ECOSYSTEM.md`, `docs/GETTING_STARTED.md`, or `docs/TRACEABILITY.md`, although those live in the same folder.
3. **`docs/FACADE_GUIDE.md` vs `docs/facades/README.md` vs `docs/facades/migration-guide.md`** — three facade docs at different paths, scope overlap. Need a single facade landing page with clear subtopic delegation.
4. **`docs/SOUP.md`** — declares SSOT for SOUP list but is not linked from `docs/README.md` index.
5. **`docs/UNIFIED_API_GUIDE.md` and `docs/guides/MIGRATION_UNIFIED_API.md`** cover related topics but do not cross-reference each other.
6. **`docs/adr/ADR-003-protocol-modularization.md`** and `docs/MIGRATION.md` (Monolithic→Modular) cover the same decision from different angles but are not cross-linked.
7. **`docs/advanced/CONCEPTS.md` and `docs/implementation/01-architecture-and-components.md`** overlap on architecture layers without cross-reference.
8. **`docs/guides/tracing.md`** not linked from top-level `docs/README.md` or from `docs/integration/with-monitoring.md`.
9. **`docs/HTTP2_GUIDE.md` and `docs/advanced/HTTP_ADVANCED.md`** — overlap; `HTTP2_GUIDE.md` is a protocol-specific feature doc and `HTTP_ADVANCED.md` is a tuning doc. Clarify with mutual links.
10. **`docs/DTLS_RESILIENT_GUIDE.md`** not linked from `docs/TLS_SETUP_GUIDE.md` (TLS and DTLS are related).
11. **`docs/advanced/MIGRATION.md` and `docs/migration/adapter_to_bridge_migration.md`** cover overlapping migration topics without cross-links.
12. **Korean counterpart divergence** — `docs/README.kr.md` has last-updated of 2025-11-28 and lists compiler requirements (`GCC 9+`, `Clang 10+`) that have NEVER appeared in `docs/README.md` (which references `docs/` index). The Korean README is effectively a separate document, not a translation.

---

## Nice-to-Have Items

### Phase 2 (4)

1. **Consistent "Last Updated" format** — some docs use `**Last Updated**:`, some `*Last Updated:`, some `> **Last Updated:**`, some `Date:`, some `**Date**:`. Standardize.
2. **Heading capitalization** — Title Case vs Sentence case is mixed within the same doc (e.g., `docs/FEATURES.md` has both "Implementation Features" and "Core design principles").
3. **Broken-target "near-miss" suggestions** — Many of the 91 broken inter-file links have an obvious sibling; a rewrite pass with semi-automated path correction would resolve ~70% in one batch.
4. **Korean README compiler footprint** — `docs/README.kr.md:208`: "C++ 표준: C++20 (일부 기능은 C++17 호환)" claims C++17 compatibility; not obvious elsewhere. Either substantiate or remove.

### Phase 3 (8)

1. **Orphan files** — 17 files not linked from any other markdown:
   - `docs/ECOSYSTEM.md`
   - `docs/GETTING_STARTED.md`
   - `examples/README.md`
   - `include/kcenon/network/core/README.md`
   - `include/kcenon/network/experimental/README.md`
   - `include/kcenon/network/http/README.md`
   - `libs/network-all/README.md`
   - `libs/network-core/README.md`
   - `libs/network-grpc/README.md`
   - `libs/network-http2/README.md`
   - `libs/network-quic/README.md`
   - `libs/network-tcp/README.md`
   - `libs/network-udp/README.md`
   - `libs/network-websocket/README.md`
   - `network-core/README.md` (also a duplicate of `libs/network-core/README.md`)
   - `package/vcpkg/README.md`
   - `samples/migration/README.md`
   The `libs/*/README.md` files are intentional per-package docs — fine to stay standalone. `docs/ECOSYSTEM.md` and `docs/GETTING_STARTED.md` should be linked from `docs/README.md`.
2. **`docs/README.md` index completeness** — Missing: ECOSYSTEM, GETTING_STARTED, TRACEABILITY, SOUP, FACADE_GUIDE, tracing, CONCEPTS.
3. **CHANGELOG dual existence** — `/CHANGELOG.md` vs `docs/CHANGELOG.md` (see Must-Fix P3-6); if intentional, document rationale in both.
4. **ADR cross-linking** — Add "Prior ADR / Next ADR" navigation to the three ADRs.
5. **`docs/migration/*.md`** would benefit from a `docs/migration/README.md` landing page.
6. **Doxygen `.dox` files** — `docs/mainpage.dox`, `docs/faq.dox`, `docs/troubleshooting.dox` not in scope for markdown review but have overlapping content with `.md` equivalents; consider consolidating or delegating.
7. **`CLAUDE.md` at root** — developer-facing AI config file; contains technical claims (`ASIO >= 1.30.2`) that disagree with user-facing docs. Decide whether this file is authoritative or informational.
8. **Redundant separator formatting** — multiple `---` horizontal rules used inconsistently across files; not functional but affects readability.

---

## Score

- **Overall**: 6.0/10
- **Anchors (P1)**: 6.5/10 — 126 broken out of 1,189 links (~10.6%); high concentration in 4 hotspot files (`docs/implementation/01-*.md` alone has 34).
- **Accuracy (P2)**: 6.0/10 — no fictional years or future-dated doom, but pervasive version/requirement drift (CMake, GCC, Clang, ASIO) and committed absolute paths.
- **SSOT (P3)**: 5.5/10 — strong split docs for Architecture & API Reference, but three competing "Integration Guide" SSOT claims, seven fragmented migration SSOTs, and a duplicated `network-core/README.md` at two paths with divergent content. Orphan rate acceptable once library READMEs are excluded.

---

## Notes

### Scope & methodology

- Files scanned: all `*.md` under `/Volumes/T5 EVO/Sources/network_system` except `build/`, `.git/`, `node_modules`. 117 files total. Templates under `.github/ISSUE_TEMPLATE/` and `.github/pull_request_template.md` included.
- Anchor resolution: GitHub-style slug (lowercase, strip non-word chars except `-`/`_`/space, spaces→hyphens; do NOT strip leading/trailing hyphens; do NOT collapse runs → matches observed GitHub behavior on emoji-prefixed headings).
- Duplicate heading suffixes `-1`, `-2`, ... are registered but not encountered as failures in this corpus.
- External URLs (schemes with `:`) skipped; only `#anchor`, `./path.md`, `./path.md#anchor`, `../path/file.md` forms validated.
- Code fences (`` ``` ``, `~~~`) exclude their contents from link/heading extraction. Two files have unbalanced fences — see Should-Fix.
- Inline code (single backticks) masked before link extraction to avoid false positives from C++ lambdas like `[](int)`.

### Known-limitations of this review

- Not all external URLs were HEAD-checked for HTTP 200.
- No `.dox` / Doxygen file validation (out of markdown scope).
- SSOT contradictions judged by SSOT-declaration text only; deeper semantic overlap (e.g., prose duplication between English/Korean variants) not line-diffed.
- Korean-text accuracy (translation quality) not reviewed beyond surface version-number mismatches with English siblings.

### Recurring patterns

1. **Path-scoping errors** (~49 items) — Authors reference peer docs by bare filename (`TLS_SETUP_GUIDE.md`, `TROUBLESHOOTING.md`, `API_REFERENCE.md`) expecting same-directory resolution, but the target lives in a different subdirectory (`docs/guides/`, `docs/advanced/`). A single pass updating all such links would clear most Phase 1 inter-file breakage.
2. **TOC-without-content / unfinished docs** — `docs/implementation/01-architecture-and-components.md` has a 40+ item TOC but only 3 real sections implemented; 34 anchors dangle. Similar TOC-vs-heading skew in `docs/guides/TLS_SETUP_GUIDE{.md,.kr.md}` for 12 anchors.
3. **Fragmented SSOT claims** — 7 migration docs + 3 Integration Guide claimants + 2 API Reference structures (split EN vs monolithic KR). A global SSOT audit naming one canonical doc per topic would resolve most Phase 3 findings.

### Machine-readable results

Phase 1 full broken list JSON: `/tmp/claude-501/docreview/p1_network_v3.json` (generated during analysis; not committed).

---

## Post-Fix Re-Validation (2026-04-15)

**Scope**: Phase 1 only (anchors and file links). Phases 2 and 3 are not re-run here.

**Fix commit**: `2c607857` — `docs: fix 60+ broken links, unify toolchain versions, sanitize paths` (52 files changed, +518 / -399).

**Methodology**: Same as the original Phase 1 scan (GitHub-style slugs, duplicate-suffix registry, fenced code blocks excluded, inline code masked, external URLs skipped). Additionally, `<!-- ... -->` HTML comments are stripped before link extraction so that explicit TODO markers introduced by the fix are correctly ignored.

### Before / After

| Metric                               | Before (2026-04-14) | After (2026-04-15) | Delta |
|--------------------------------------|---------------------|--------------------|-------|
| Files scanned                        | 117                 | 118                |   +1  |
| Phase 1 Must-Fix (anchors + links)   | 126                 |   0                | -126  |
| Broken inter-file file links         |  91                 |   0                |  -91  |
| Broken intra-file anchors            |  35                 |   0                |  -35  |
| Broken inter-file anchors            |   0                 |   0                |    0  |
| `/Users/raphaelshin/...` paths in `docs/advanced/MIGRATION.md` | 11 |   0 |  -11  |

Files-scanned went from 117 to 118 because the fix commit added `DOC_FIX_SUMMARY_2026-04-15.md` (root-level log of the fix, not excluded by `build/` / `.git/` filters).

### `/Users/raphaelshin/` Removal Verification

`grep -r "/Users/raphaelshin" --include="*.md"` against the entire repository (excluding `build/` and `.git/`) returns **only** the two occurrences inside `DOC_REVIEW_REPORT.md` itself (this report's original Must-Fix description). Zero remaining occurrences in `docs/advanced/MIGRATION.md` or any other source document. All 11 previously committed absolute paths (original lines 1477–1491 and 1539) have been replaced with repository-relative markdown links (e.g., `[samples/basic_usage.cpp](../../samples/basic_usage.cpp)`) and canonical GitHub URLs for ecosystem repos. **Verified: all dev-machine username leakage removed.**

### Residual Must-Fix Items (Phase 1)

**None.** The scanner found 0 broken intra-file anchors, 0 broken inter-file file links, and 0 broken inter-file anchors across all 118 files.

Spot checks confirm correctness:
- `README.md:690–707` — all seven doc links (`docs/ARCHITECTURE.md`, `docs/API_REFERENCE.md`, `docs/guides/BUILD.md`, `docs/MIGRATION.md`, `docs/guides/TLS_SETUP_GUIDE.md`, `docs/guides/TROUBLESHOOTING.md`, `docs/guides/LOAD_TEST_GUIDE.md`) resolve to existing files.
- `README.md:908,919` — previously-broken `IMPROVEMENTS.md` and `CODING_STYLE_RULES.md` references are now wrapped in `<!-- TODO: ... -->` markers (properly excluded from link validation) with an in-text fallback to `docs/contributing/CONTRIBUTING.md`.
- `docs/UNIFIED_API_GUIDE.md:23–25` — the three `#i_transport-interface`, `#i_connection-interface`, `#i_listener-interface` intra anchors now match existing `## i_transport Interface` / `## i_connection Interface` / `## i_listener Interface` headings (GitHub preserves underscores in slugs).
- `docs/implementation/01-architecture-and-components.md:20–29` — the TOC is now pruned to the 6 sections that actually exist (`#1-module-layer-structure`, `#2-namespace-design`, `#-core-component-implementation`, `#1-messaging_bridge-class`, `#2-core-api-design`, `#3-session-management`); the formerly-dangling 34 TOC entries are moved inside a `<!-- TODO: ... -->` block.
- `docs/advanced/MIGRATION.md:1477–1491,1539` — all 11 `/Users/raphaelshin/Sources/network_system/...` paths replaced with `../../samples/...`, `../API_REFERENCE.md`, `../../CHANGELOG.md`, and `https://github.com/kcenon/...` equivalents.

### Regression Items (Phase 1)

**None detected.** The fix commit did not introduce any new broken anchors, empty link stubs, or missing-file references. TODO markers were consistently placed inside HTML comments, keeping them out of the link-validation surface while preserving human-readable roadmap notes.

### Methodology Note on Slug Algorithm

During this re-validation an earlier draft of the scanner incorrectly stripped `_` as part of markdown-inline formatting, which produced 6 false positives (all in `docs/UNIFIED_API_GUIDE.md`, `docs/advanced/CONCEPTS.md`, `docs/implementation/01-*.md` where headings contain underscored identifiers like `i_transport`, `common_system`, `messaging_bridge`). The fix was to limit the inline-formatting strip to `* ` / `` ` `` / `~`, matching GitHub's behavior that preserves underscores in anchor slugs. After this correction, zero Phase 1 issues remained. The ORIGINAL report's 126-item count used a different tool chain and is not affected by this scanner bug; the re-validation count of 0 is authoritative for the post-fix state.

### Verdict

**Phase 1: PASS.** All 126 originally-identified Must-Fix items are resolved. Zero residuals, zero regressions. All 11 committed absolute `/Users/raphaelshin/...` paths are fully removed from `docs/advanced/MIGRATION.md` and replaced with portable relative links. Phase 2 (accuracy) and Phase 3 (SSOT & redundancy) items listed earlier in this report are out of scope for this re-run and remain open for subsequent passes.

---

*End of report.*
