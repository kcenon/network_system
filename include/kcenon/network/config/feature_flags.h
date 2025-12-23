// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * @file feature_flags.h
 * @brief Feature flags for network_system
 *
 * This file provides unified feature detection for network_system.
 * It works both with and without common_system dependency.
 *
 * When common_system is available (WITH_COMMON_SYSTEM defined),
 * this file includes common_system's feature_flags.h.
 *
 * When common_system is not available, this file defines the
 * KCENON_WITH_* macros locally based on CMake definitions.
 *
 * @author kcenon
 * @date 2025
 */

#pragma once

//==============================================================================
// Check for common_system availability
//==============================================================================

#if defined(WITH_COMMON_SYSTEM) || defined(KCENON_WITH_COMMON_SYSTEM)
    // common_system is available, use its feature flags
    #include <kcenon/common/config/feature_flags.h>
#else
    // common_system is not available, define feature flags locally

//==============================================================================
// Thread System Integration
//==============================================================================

#ifndef KCENON_WITH_THREAD_SYSTEM
    #if defined(ENABLE_THREAD_INTEGRATION) || defined(WITH_THREAD_SYSTEM)
        #define KCENON_WITH_THREAD_SYSTEM 1
    #else
        #define KCENON_WITH_THREAD_SYSTEM 0
    #endif
#endif

//==============================================================================
// Logger System Integration
//==============================================================================

#ifndef KCENON_WITH_LOGGER_SYSTEM
    #if defined(ENABLE_LOGGER_INTEGRATION) || defined(WITH_LOGGER_SYSTEM)
        #define KCENON_WITH_LOGGER_SYSTEM 1
    #else
        #define KCENON_WITH_LOGGER_SYSTEM 0
    #endif
#endif

//==============================================================================
// Monitoring System Integration
//==============================================================================

#ifndef KCENON_WITH_MONITORING_SYSTEM
    #if defined(ENABLE_MONITORING_INTEGRATION) || defined(WITH_MONITORING_SYSTEM)
        #define KCENON_WITH_MONITORING_SYSTEM 1
    #else
        #define KCENON_WITH_MONITORING_SYSTEM 0
    #endif
#endif

//==============================================================================
// Container System Integration
//==============================================================================

#ifndef KCENON_WITH_CONTAINER_SYSTEM
    #if defined(ENABLE_CONTAINER_INTEGRATION) || defined(WITH_CONTAINER_SYSTEM)
        #define KCENON_WITH_CONTAINER_SYSTEM 1
    #else
        #define KCENON_WITH_CONTAINER_SYSTEM 0
    #endif
#endif

//==============================================================================
// Common System Integration
//==============================================================================

#ifndef KCENON_WITH_COMMON_SYSTEM
    #define KCENON_WITH_COMMON_SYSTEM 0
#endif

#endif // WITH_COMMON_SYSTEM
