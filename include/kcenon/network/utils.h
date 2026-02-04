/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#pragma once

/*!
 * \file utils.h
 * \brief Unified utility header for network_system
 *
 * This umbrella header provides convenient access to all public utility
 * components in network_system. It includes:
 * - Error handling and result types (result_types.h)
 * - Lifecycle management (lifecycle_manager.h, startable_base.h)
 * - Callback management (callback_manager.h)
 *
 * Usage:
 * \code
 * #include <kcenon/network/utils.h>
 *
 * using namespace kcenon::network;
 *
 * // Use Result<T> for error handling
 * auto result = some_operation();
 * if (result.is_ok()) {
 *     process(result.value());
 * }
 *
 * // Use lifecycle_manager for component management
 * lifecycle_manager lifecycle;
 * lifecycle.start();
 * \endcode
 *
 * \note Internal utilities (buffer_pool, compression_pipeline, etc.) are not
 * included in this header and should be accessed directly from src/internal/
 * when needed for internal implementation.
 *
 * \see docs/architecture/facade-pattern.md
 */

// Core utilities (from detail directory)
#include "kcenon/network/detail/utils/result_types.h"

// Lifecycle management (from detail directory)
#include "kcenon/network/detail/utils/lifecycle_manager.h"
#include "kcenon/network/detail/utils/startable_base.h"

// Callback management (from detail directory)
#include "kcenon/network/detail/utils/callback_manager.h"

/*!
 * \namespace kcenon::network::utils
 * \brief Utility components for network_system
 *
 * This namespace contains essential utility components:
 * - **Result types**: Error handling with Result<T>/VoidResult pattern
 * - **Lifecycle management**: Component start/stop lifecycle patterns
 * - **Callback management**: Type-safe callback registration and invocation
 *
 * All utilities use modern C++ patterns and are designed for:
 * - Type safety with minimal overhead
 * - Thread-safe operations where applicable
 * - Clear error reporting through Result<T>
 */
