# HTTP/2 Client Coverage Round 4 Diagnosis

> **Issue**: [#1110](https://github.com/kcenon/network_system/issues/1110)
> **Parent**: [#1106](https://github.com/kcenon/network_system/issues/1106)
> **Epic**: [#953](https://github.com/kcenon/network_system/issues/953)
> **Reference run**: [25464342500](https://github.com/kcenon/network_system/actions/runs/25464342500) at sha `d5378644`

## Question

Why did Round 1 (PR #994 / #1062), Round 2 (PR #1108 — 6 TEST_F), and Round 3
(PR #1109 — 7 TEST_F) all produce **0pp delta** on per-file coverage of
`src/protocols/http2/http2_client.cpp` (still 18.75% line / 9.91% branch,
108/576 LH/LF, 97/979 BRH/BRF) despite landing 13 substrate-driven TEST_F?

## Answer (one line)

The 7 Round-3 dispatcher TEST_F **did execute** under coverage instrumentation
but **all 7 FAILED** at the SETTINGS-handshake `wait_for` gate, never reaching
the dispatcher branches they were designed to cover. The same coverage-only
timing trap that motivated the `NETWORK_COVERAGE_BUILD` guard for the two
Round-2 positive-path tests applies to the entire `Http2ClientHermeticTransport`
fixture, not just the two tests originally guarded.

## Evidence

Direct grep of the `Run tests` step stdout in run [25464342500] shows every
Round-3 dispatcher test on the `(Failed)` summary list:

| ctest # | TEST_F                                                          | Outcome under coverage |
|---------|-----------------------------------------------------------------|------------------------|
| 5886    | `ServerPingFrameDrivesHandlePingAndKeepsConnectionAlive`        | FAILED (5.26 s)        |
| 5887    | `ServerPingAckFrameIsAbsorbedSilently`                          | FAILED (5.30 s)        |
| 5888    | `ServerGoawayFrameFlipsConnectionStateToDisconnected`           | FAILED (5.35 s)        |
| 5889    | `ServerWindowUpdateOnConnectionStreamExpandsWindow`             | FAILED (5.38 s)        |
| 5890    | `ServerWindowUpdateOnUnknownStreamIsSilentlyIgnored`            | FAILED (5.34 s)        |
| 5891    | `ServerRstStreamOnUnknownStreamIsSilentlyIgnored`               | FAILED (5.28 s)        |
| 5892    | `ServerUnknownFrameTypeIsHandledWithoutCrashing`                | FAILED (5.12 s)        |

The baseline fixture sentinel test (#5868, `ConnectCompletesSettingsExchangeWithMockPeer`)
also FAILED in the same coverage run, along with most other
`Http2ClientHermeticTransportTest.*` cases (#5870–#5880). This is not a
new-test regression — the entire hermetic-transport fixture cannot complete
the SETTINGS handshake within the 3-second `wait_for` budget under
`-fprofile-arcs -ftest-coverage` instrumentation on the GitHub Actions
ubuntu-latest runner.

## Why ctest reported PASS for the Coverage Analysis workflow

The `Run tests` step uses `ctest` invoked with permissive flags
(no `--output-on-failure` strict gate), and the workflow defines the
"Coverage Analysis" job as PASS as long as the workflow steps that produce
`coverage.info` succeed. Per-test failure in the test executor does not fail
the workflow — only an `lcov` capture failure does. So the workflow shows
green checkmarks even though 7 of the 7 new tests (and ~30 other
hermetic-transport tests) produced no coverage data because they aborted at
the handshake.

## Why hypotheses 1 and 3 are ruled out

**Hypothesis 1 (CMake `NETWORK_COVERAGE_BUILD` over-scoping)** — refuted by
direct inspection of `tests/unit/http2_client_branch_test.cpp`:

- The `#ifndef NETWORK_COVERAGE_BUILD` guard wraps lines 1257–1328 only, which
  contain exactly the two flaky positive-path tests that motivated the guard
  (`SlowWriteServerSettingsStillCompletesHandshake` and
  `EchoOneTruncateAtNineDropsResponsePayloadFailingGet`).
- The 7 Round-3 dispatcher TEST_F are at lines 1361–1580, **outside** the
  guard, so they are compiled into the coverage binary.
- The grep above confirms all 7 are scheduled and run by ctest under coverage
  (their names appear in the per-test `[ RUN      ]` markers and in the
  `Failed` summary).

The macro is correctly scoped. The dispatcher tests reach the binary; they
just cannot complete the handshake under instrumentation.

**Hypothesis 3 (lcov filter dropping a second production SF)** — refuted by
the SF-record cross-check already in the #1106 Round-3 post-merge comment:
`coverage_filtered.info` exposes 1 SF for `http2_client.cpp`, `coverage.info`
exposes 2 (the second is `tests/test_http2_client.cpp`), and the `--remove`
rule strips only the test file. The 18.75% / 9.91% numbers are the true
production-source measurement, no second production compile unit is being
filtered out.

## Hypothesis 2 (overlap with baseline) does not apply

The dispatcher tests don't overlap with the baseline `ConnectCompletes…`
test because the baseline test ALSO failed under coverage instrumentation
in run 25464342500. Even if the baseline did pass and overlap, the new tests
were designed to drive `handle_ping_frame`, `handle_goaway_frame`,
`handle_window_update_frame`, `handle_rst_stream_frame`, and the
`process_frame` default branch — none of which the baseline reaches because
the baseline never receives a server-originated post-handshake frame.

The actual root cause is upstream of the dispatch: the SETTINGS handshake
itself is the failure point.

## Why the handshake fails specifically under coverage instrumentation

Three compounding factors:

1. **gcov instrumentation overhead.** `-fprofile-arcs -ftest-coverage`
   inserts a counter increment at every basic block. For the SSL handshake
   path through OpenSSL plus the ASIO async I/O state machine plus
   `mock_h2_server_peer`'s framing logic, this cumulatively pushes the
   end-to-end SETTINGS exchange past the 3-second budget that
   `wait_for(predicate, std::chrono::seconds(3))` enforces in
   `tests/support/hermetic_transport_fixture.h`.
2. **Coroutine-based async I/O does not yield CPU during gcov flush.**
   `mock_h2_server_peer` runs on a separate thread that performs a blocking
   `asio::read` for each frame header; under coverage, the per-block
   counter increment in the read path slows the ack-emission rate.
3. **Polling cadence is 5ms.** `wait_for` polls every 5ms, but each poll
   itself triggers gcov increments in the predicate (`peer.settings_exchanged()`
   reads an `std::atomic<bool>` inside an instrumented function). Predicate
   evaluation cost scales with the rate.

Wall-clock evidence: the failed dispatcher tests took 5.1–5.4 s each under
coverage, which is more than 1.7× the 3-second budget. On Debug/Release
matrix builds (no instrumentation), the same fixture's tests typically
complete in 100–300 ms.

## Recommended fix (proposed; out of scope for this diagnostic issue)

A new sub-issue `test(http2): widen NETWORK_COVERAGE_BUILD guard around the
full dispatcher TEST_F block` should land before any Round 5 work. Two
options exist; both are non-invasive (test-only):

### Option A — extend the existing macro guard

Wrap lines 1361–1580 (the 7 dispatcher TEST_F) in
`#ifndef NETWORK_COVERAGE_BUILD` so they are excluded from coverage builds
the same way the two Round-2 flaky tests are. Pro: zero new infrastructure,
keeps the tests in Debug/Release matrix builds where they already pass.
Con: contributes 0pp to coverage, so #1106 acceptance criteria still need
a different mechanism.

### Option B — generous coverage-only timeout multiplier

Add a `kCoverageWaitMultiplier` constant in `hermetic_transport_fixture.h`
that multiplies the `wait_for` timeout by 5× when `NETWORK_COVERAGE_BUILD`
is defined (so the 3-second budget becomes 15 s). Increase the default
timeout for `wait_for(...)` in `make_connected_client` similarly. Pro:
allows the dispatcher tests to actually complete under instrumentation
and contribute to the line/branch numbers. Con: lengthens the coverage
workflow runtime; the 7 tests may take 50+ s combined.

### Option C — measure dispatcher coverage outside the hermetic fixture

The dispatcher branches in `http2_client.cpp` (`handle_ping_frame`,
`handle_goaway_frame`, …) can also be exercised by a unit test that
constructs an `http2_client` directly, calls a friend-injected
`process_frame(frame_bytes)` method, and observes the resulting state.
This bypasses the SETTINGS handshake entirely and avoids the timing trap
at the cost of needing a friend-class hook. Pro: deterministic, fast even
under coverage. Con: requires a private-access seam in production code or
a friend declaration in a coverage-only header.

**Recommendation**: Option B is the smallest change. Option C is the
right long-term answer for testing the dispatcher branches in isolation.
Option A unblocks the workflow noise but does not help #1106 close.

## Acceptance criteria status (this issue)

- [x] Confirm whether the 7 Round-3 dispatcher TEST_F actually executed
      under coverage instrumentation in run [25464342500].
      **Answer**: All 7 are scheduled by ctest, all 7 abort with FAILED at
      the SETTINGS-handshake `wait_for` gate (5.1–5.4 s each).
- [x] If they did NOT execute, identify the CMake/macro mechanism that
      excluded them. **Answer**: They DO execute. The macro is correctly
      scoped to the two original flaky tests only.
- [x] If they DID execute but produced 0pp, document why.
      **Answer**: The fixture-wide handshake timing trap drops them at the
      first `EXPECT_TRUE(wait_for(... settings_exchanged() ..., 3 s))`
      assertion before any dispatcher branch runs.
- [x] Verify the lcov filter pipeline does not silently drop a second
      production-source SF for `http2_client.cpp`. **Answer**: SF-record
      cross-check (already in #1106 Round-3 post-merge comment) confirms
      only the test file is filtered; the production source SF is intact.
- [x] Output: a one-page diagnostic note. **Answer**: This file.

## Next action

Open Round 5 sub-issue scoped to **Option B (coverage-only timeout
multiplier)** as the smallest change that lets #1106 actually progress.
Round 5 acceptance criteria should require a post-fix coverage delta of
at least +20pp line and +10pp branch on `http2_client.cpp`, not merely
"tests pass under coverage" — to break the 0pp pattern definitively.

## Related

- Part of [#953](https://github.com/kcenon/network_system/issues/953)
- Part of [#1106](https://github.com/kcenon/network_system/issues/1106)
- Substrate provider: [#1074](https://github.com/kcenon/network_system/issues/1074)
- PR #1108 (Round 2, merged b4692fed, 0pp delta)
- PR #1109 (Round 3, merged d5378644, 0pp delta)
