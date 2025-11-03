#pragma once

/**
 * @file thread_system_adapter.h
 * @brief Direct integration with thread_system
 *
 * This file is maintained for backward compatibility.
 * New code should include <kcenon/thread/core/thread_pool.h> directly
 * and use thread_pool_manager for pool management.
 *
 * @author kcenon
 * @date 2025-09-20
 * @note DEPRECATED: Include thread_system headers directly
 */

#include <kcenon/thread/core/thread_pool.h>
#include <kcenon/thread/core/typed_thread_pool.h>

namespace network_system::integration {

/**
 * @brief Type aliases for thread_system types
 *
 * @deprecated Include thread_system headers directly
 */

// Thread pool types
using thread_pool = kcenon::thread::thread_pool;
template<typename T = kcenon::thread::job_types>
using typed_thread_pool = kcenon::thread::typed_thread_pool_t<T>;

// Worker types
using thread_worker = kcenon::thread::thread_worker;
template<typename T = kcenon::thread::job_types>
using typed_thread_worker = kcenon::thread::typed_thread_worker_t<T>;

// Job types
using job = kcenon::thread::job;
template<typename T = kcenon::thread::job_types>
using typed_job = kcenon::thread::typed_job_t<T>;

// Result types
using result_void = kcenon::thread::result_void;

} // namespace network_system::integration
