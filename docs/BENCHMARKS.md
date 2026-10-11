---
doc_id: "NET-PERF-002"
doc_title: "Network Benchmark Evidence"
doc_version: "1.1.0"
doc_date: "2026-10-11"
doc_status: "Released"
project: "network_system"
category: "PERF"
---

# Network Benchmark Evidence

[한국어](BENCHMARKS.kr.md)

Documentation review date: 2026-10-11. This is not a measurement date.
No benchmarks were rerun for this documentation change.

## Evidence status

This page replaces headline performance claims with an inventory of what the
repository actually records. A benchmark source is not a result artifact, and a
workflow badge does not identify a measurement environment or prove a performance threshold.

| Record | Available evidence | Limitations |
|--------|--------------------|-------------|
| Historical message table below | Archived documentation and message benchmark source | Original run date, exact command, commit, and raw output are unavailable |
| Mock connection/session operations | [connection source](../benchmarks/connection_bench.cpp), [session source](../benchmarks/session_bench.cpp) | Mock objects; no socket throughput claim follows |
| TCP loopback | [source](../benchmarks/tcp_loopback_bench.cpp) | Real socket I/O; this page has no matched result artifact |
| Facade versus direct API | [source](../benchmarks/facade_vs_direct_bench.cpp) | Existence of a comparison benchmark does not establish equal performance |
| Prior Korean TCP, scaling, Boost.Asio/libuv comparison tables | Earlier documentation | No attributable raw records found; unsupported values removed |

## Synthetic benchmarks

### Unverified historical summary

The following values are transcribed from the [previous English document](https://github.com/kcenon/network_system/blob/0a815e75631a55f8a8be54567e612d89fecb78ff/docs/BENCHMARKS.md#synthetic-benchmarks).
They are retained as historical documentation, not validated current results or
network throughput. Their original measurement provenance cannot be reconstructed
from the repository records inspected for this change.

| Metadata | Historical record |
|----------|-------------------|
| Reported hardware | Intel i7-12700K at 3.2 GHz, 32 GB DDR4-3200, NVMe SSD |
| Reported software | Ubuntu 22.04 LTS, GCC 11.3, C++20, Release with -O3 |
| Reported scope | CPU-only allocation/copy; no socket I/O |
| Measurement date | Unknown; the old page’s 2025-11-15 update and 2026-04-04 metadata dates are document dates |
| Original command / source revision | Not recorded |
| Original result artifact | Not located; the archived Markdown table is a summary, not raw output |
| Source available today | [message_throughput_bench.cpp](../benchmarks/message_throughput_bench.cpp) |

| Historical payload label | Reported time per operation (ns) | Reported throughput (msg/s) |
|--------------------------|----------------------------------|-----------------------------|
| 64 bytes | 1,300 | ~769,000 |
| 256 bytes | 3,270 | ~305,000 |
| 1 KB | 7,803 | ~128,000 |
| 8 KB | 48,000 | ~21,000 |

Current benchmark registrations do not map every historical payload row to a named
case. Do not infer that today's source reproduces this table. In particular, current
copy/serialization registrations use different size sets. GCC 11.3 is the compiler
reported for the historical record; current source requirements are in the
[build guide](guides/BUILD.md). The unsupported Apple M1 platform claim is removed.

### Connection benchmarks

The connection source uses mock operations and simulated delays. Earlier numeric
summaries had no attributable raw run and are omitted; they cannot establish TCP
handshake or network throughput results.

### Session benchmarks

The session source measures mock bookkeeping operations. It does not measure
end-to-end protocol performance. No current measured table is asserted here.

## Real network load tests

The TCP loopback source includes connection establishment, echo round trips, and
stream transfer through sockets. These have different scopes from CPU-only benchmarks.
The [integration tests](../integration_tests/) also exercise network behavior;
test execution alone is not an attributable performance result.

## Performance comparison

No supported facade/direct equality or cross-library ranking is asserted. A future
comparison needs matched environments, revisions, workloads, and raw outputs for
both implementations. Prior unsupported rankings are not carried forward.

## Benchmark methodology

For each new reported measurement, record the source commit, measurement date,
CPU/OS/compiler, build flags, dependency versions, payload and connection counts,
exact command, repetition/aggregation policy, and a durable raw-result link.
Specify whether the measured operation is CPU-only, mocked, or real I/O.
Keep document review dates separate from measurement dates.

## Reproducing benchmarks

The following is a **suggested new capture procedure**, not the original command
for the historical table and not a procedure executed for this documentation PR.
First follow the [source build prerequisites](guides/BUILD.md). Install Google
Benchmark separately: the [CMake configuration](../benchmarks/CMakeLists.txt)
skips its target when the package is absent; enabling the option does not install it.

```bash
cmake -S . -B build/perf -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/dependency/prefix;/path/to/google-benchmark/prefix" \
  -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF -DNETWORK_BUILD_BENCHMARKS=ON \
  -DKCENON_WITH_LOGGER_SYSTEM=OFF -DKCENON_WITH_CONTAINER_SYSTEM=OFF
cmake --build build/perf --target network_benchmarks
./build/perf/benchmarks/network_benchmarks \
  --benchmark_filter='BM_Message_(Create|Copy|Serialize)' \
  --benchmark_repetitions=5 --benchmark_out_format=json \
  --benchmark_out=build/perf/message-results.json
```

The result path above will exist only after a successful future run. It is not
present evidence. Review current source/toolchain compatibility before collecting
results; this change does not repair benchmark implementations.
The [benchmark workflow](../.github/workflows/benchmarks.yml) uploads run artifacts
and excludes selected network-dependent cases. Use a particular run and its actual
artifact when citing new results, rather than a generic workflow link.
