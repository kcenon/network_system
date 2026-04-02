// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
// DEPRECATED: Since issue #342, network_system uses EventBus-based metric
// publishing instead of compile-time monitoring_system dependency.
// External consumers should subscribe to network_metric_event via EventBus.
//
// This macro is kept for backward compatibility but is no longer used
// in network_system headers. It will be removed in a future version.
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
