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

#include "thread_pool_manager.h"
#include <vector>
#include <cstdint>
#include <future>
#include <functional>

namespace network_system::integration
{
    // Note: pipeline_priority enum is defined in thread_pool_manager.h

    /*!
     * \namespace pipeline_jobs
     * \brief Helper functions for submitting pipeline jobs with priorities
     *
     * These functions provide convenient wrappers for common pipeline operations,
     * automatically submitting them to the typed_thread_pool with appropriate
     * priorities.
     *
     * ### Example Usage
     * \code
     * std::vector<uint8_t> data = {1, 2, 3, 4, 5};
     *
     * // Submit encryption job (HIGH priority)
     * auto future = pipeline_jobs::submit_encryption(data);
     * auto encrypted = future.get();
     *
     * // Submit compression job (NORMAL priority)
     * auto compressed_future = pipeline_jobs::submit_compression(data);
     * auto compressed = compressed_future.get();
     * \endcode
     */
    namespace pipeline_jobs
    {
        /*!
         * \brief Submit encryption job with HIGH priority
         *
         * \param data Data to encrypt
         * \return Future containing encrypted data
         *
         * Submits encryption task to the pipeline pool with HIGH priority,
         * ensuring security-sensitive operations are processed before
         * less critical tasks.
         */
        auto submit_encryption(const std::vector<uint8_t>& data)
            -> std::future<std::vector<uint8_t>>;

        /*!
         * \brief Submit compression job with NORMAL priority
         *
         * \param data Data to compress
         * \return Future containing compressed data
         *
         * Submits compression task to the pipeline pool with NORMAL priority,
         * allowing standard data processing without urgency.
         */
        auto submit_compression(const std::vector<uint8_t>& data)
            -> std::future<std::vector<uint8_t>>;

        /*!
         * \brief Submit serialization job with NORMAL priority
         *
         * \param data Data to serialize
         * \return Future containing serialized data
         *
         * Submits serialization task to the pipeline pool with NORMAL priority.
         */
        auto submit_serialization(const std::vector<uint8_t>& data)
            -> std::future<std::vector<uint8_t>>;

        /*!
         * \brief Submit validation job with LOW priority
         *
         * \param data Data to validate
         * \return Future containing validation result
         *
         * Submits validation task to the pipeline pool with LOW priority,
         * allowing it to be processed when system resources are available.
         */
        auto submit_validation(const std::vector<uint8_t>& data)
            -> std::future<bool>;

        /*!
         * \brief Submit custom pipeline job with specified priority
         *
         * \tparam R Return type of the task
         * \param task Task function to execute
         * \param priority Job priority level
         * \return Future containing result
         *
         * Generic function for submitting custom pipeline tasks with
         * user-specified priority levels.
         *
         * ### Example
         * \code
         * auto task = []() { return compute_hash(data); };
         * auto future = pipeline_jobs::submit_custom<std::string>(
         *     task, pipeline_priority::REALTIME);
         * auto hash = future.get();
         * \endcode
         */
        template<typename R>
        auto submit_custom(std::function<R()> task,
                          pipeline_priority priority)
            -> std::future<R>;

    } // namespace pipeline_jobs

} // namespace network_system::integration
