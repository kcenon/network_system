/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

/**
 * @file result.h
 * @brief Public header for Result<T> error handling types
 *
 * This is the public API header for result types. Use this header instead of
 * including detail/utils/result_types.h directly.
 *
 * @example
 * @code
 * #include <kcenon/network/types/result.h>
 *
 * auto connect() -> kcenon::network::Result<void> {
 *     // ... connection logic
 *     return kcenon::network::ok();
 * }
 * @endcode
 */

#pragma once

// Include the implementation from detail
// This maintains backward compatibility while providing a public API path
#include "kcenon/network/detail/utils/result_types.h"

namespace kcenon::network
{

/**
 * @brief Result type for operations that can fail
 *
 * Result<T> is the primary error handling mechanism in network_system.
 * It provides a type-safe alternative to exceptions for error handling.
 *
 * @tparam T The success value type
 *
 * @note When KCENON_WITH_COMMON_SYSTEM is enabled, this is an alias to
 *       kcenon::common::Result<T>. Otherwise, a standalone implementation
 *       is provided.
 */
// Result<T> is already defined in detail/utils/result_types.h

/**
 * @brief Alias for Result<void> for operations that return no value
 */
// VoidResult is already defined in detail/utils/result_types.h

/**
 * @brief Error information structure
 *
 * Contains error code and message for failed operations.
 */
// error_info is already defined in detail/utils/result_types.h

} // namespace kcenon::network
