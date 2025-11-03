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

#include "network_system/integration/thread_pool_manager.h"
#include <vector>
#include <functional>
#include <future>
#include <any>

namespace network_system::internal
{

	/**
	 * @brief Placeholder for pipeline job definitions
	 *
	 * This file will be fully implemented in Phase 3: Pipeline Integration.
	 * For now, it contains forward declarations to support Phase 1 infrastructure.
	 *
	 * Phase 3 will add:
	 * - pipeline_job_base: Base class for typed pipeline jobs
	 * - encryption_job: Encryption/decryption job with REALTIME priority
	 * - compression_job: Data compression job with NORMAL priority
	 * - serialization_job: Data serialization job with NORMAL priority
	 * - pipeline_submitter: Convenience methods for submitting typed jobs
	 */

	// Forward declarations for Phase 3
	class pipeline_job_base;
	class encryption_job;
	class compression_job;
	class serialization_job;
	class pipeline_submitter;

	// Placeholder implementations will be added in Phase 3

} // namespace network_system::internal
