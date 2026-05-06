// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file network_test_friends.h
 * @brief Forward declarations for friend-test probes (Issue #1074 Phase 2D).
 *
 * Production headers that grant friend access to these probe types include
 * a forward declaration of the probe under the same NETWORK_ENABLE_TEST_INJECTION
 * gate. This header keeps test-side friend declarations in one place so the
 * production headers do not need to pull in concrete probe definitions.
 */

namespace kcenon::network::tests::support
{
class quic_server_probe;
class ws_server_probe;
} // namespace kcenon::network::tests::support
