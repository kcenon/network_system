// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file internal/crypto.h
 * @brief Internal QUIC crypto utilities for network-quic library
 *
 * This header provides internal QUIC cryptographic types and utilities.
 */

// Re-export QUIC crypto types from the main library internal headers
#include "internal/protocols/quic/crypto.h"
#include "internal/protocols/quic/keys.h"
#include "internal/protocols/quic/session_ticket_store.h"
