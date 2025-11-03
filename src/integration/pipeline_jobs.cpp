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

#include "network_system/integration/pipeline_jobs.h"
#include "network_system/integration/logger_integration.h"

#include <memory>
#include <stdexcept>

namespace network_system::integration::pipeline_jobs
{
    namespace detail
    {
        // Helper to submit task to pipeline pool
        // Note: For now, we use utility pool as fallback since typed_job_t requires
        // additional header includes from thread_system/src/impl which is not public
        template<typename T>
        auto submit_to_pipeline_pool(
            std::function<T()> task,
            pipeline_priority priority,
            const std::string& task_name) -> std::future<T>
        {
            auto promise = std::make_shared<std::promise<T>>();
            auto future = promise->get_future();

#ifdef BUILD_WITH_THREAD_SYSTEM
            auto& mgr = thread_pool_manager::instance();

            // TODO: Use typed_thread_pool when typed_job.h is made publicly available
            // For now, use utility pool with priority indication in logs
            auto utility_pool = mgr.get_utility_pool();

            if (!utility_pool)
            {
                NETWORK_LOG_ERROR("[pipeline_jobs] Utility pool not initialized");
                promise->set_exception(
                    std::make_exception_ptr(
                        std::runtime_error("Utility pool not available")));
                return future;
            }

            // Submit to utility pool with priority logged
            const char* priority_str = priority == pipeline_priority::REALTIME ? "REALTIME" :
                                      priority == pipeline_priority::HIGH ? "HIGH" :
                                      priority == pipeline_priority::NORMAL ? "NORMAL" :
                                      priority == pipeline_priority::LOW ? "LOW" : "BACKGROUND";

            utility_pool->submit_task([task = std::move(task), promise, task_name, priority_str]() {
                try
                {
                    NETWORK_LOG_TRACE("[pipeline_jobs:" + std::string(priority_str) + "] Executing " + task_name);
                    T result = task();
                    promise->set_value(std::move(result));
                }
                catch (const std::exception& e)
                {
                    NETWORK_LOG_ERROR("[pipeline_jobs] " + task_name +
                                    " failed: " + std::string(e.what()));
                    promise->set_exception(std::current_exception());
                }
                catch (...)
                {
                    NETWORK_LOG_ERROR("[pipeline_jobs] " + task_name +
                                    " failed with unknown exception");
                    promise->set_exception(std::current_exception());
                }
            });
#else
            // Fallback: Execute synchronously
            NETWORK_LOG_WARN("[pipeline_jobs] thread_system not available, executing synchronously");
            try
            {
                T result = task();
                promise->set_value(std::move(result));
            }
            catch (...)
            {
                promise->set_exception(std::current_exception());
            }
#endif

            return future;
        }

        // Default encryption implementation (placeholder)
        auto encrypt_impl(const std::vector<uint8_t>& data) -> std::vector<uint8_t>
        {
            NETWORK_LOG_TRACE("[pipeline_jobs] default_encrypt_stub");
            // TODO: Implement actual encryption
            return data;
        }

        // Default compression implementation (placeholder)
        auto compress_impl(const std::vector<uint8_t>& data) -> std::vector<uint8_t>
        {
            NETWORK_LOG_TRACE("[pipeline_jobs] default_compress_stub");
            // TODO: Implement actual compression
            return data;
        }

        // Default serialization implementation (placeholder)
        auto serialize_impl(const std::vector<uint8_t>& data) -> std::vector<uint8_t>
        {
            NETWORK_LOG_TRACE("[pipeline_jobs] default_serialize_stub");
            // TODO: Implement actual serialization
            return data;
        }

        // Default validation implementation (placeholder)
        auto validate_impl(const std::vector<uint8_t>& data) -> bool
        {
            NETWORK_LOG_TRACE("[pipeline_jobs] default_validate_stub");
            // TODO: Implement actual validation
            return !data.empty();
        }

    } // namespace detail

    auto submit_encryption(const std::vector<uint8_t>& data)
        -> std::future<std::vector<uint8_t>>
    {
        return detail::submit_to_pipeline_pool<std::vector<uint8_t>>(
            [data]() { return detail::encrypt_impl(data); },
            pipeline_priority::HIGH,
            "encryption");
    }

    auto submit_compression(const std::vector<uint8_t>& data)
        -> std::future<std::vector<uint8_t>>
    {
        return detail::submit_to_pipeline_pool<std::vector<uint8_t>>(
            [data]() { return detail::compress_impl(data); },
            pipeline_priority::NORMAL,
            "compression");
    }

    auto submit_serialization(const std::vector<uint8_t>& data)
        -> std::future<std::vector<uint8_t>>
    {
        return detail::submit_to_pipeline_pool<std::vector<uint8_t>>(
            [data]() { return detail::serialize_impl(data); },
            pipeline_priority::NORMAL,
            "serialization");
    }

    auto submit_validation(const std::vector<uint8_t>& data)
        -> std::future<bool>
    {
        return detail::submit_to_pipeline_pool<bool>(
            [data]() { return detail::validate_impl(data); },
            pipeline_priority::LOW,
            "validation");
    }

} // namespace network_system::integration::pipeline_jobs
