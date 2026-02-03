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

#include "kcenon/network/utils/resilient_client.h"

#include <thread>

#include "internal/integration/logger_integration.h"

namespace kcenon::network::utils
{

	resilient_client::resilient_client(const std::string& client_id,
									   const std::string& host,
									   unsigned short port,
									   size_t max_retries,
									   std::chrono::milliseconds initial_backoff
#ifdef WITH_COMMON_SYSTEM
									   , common::resilience::circuit_breaker_config cb_config
#endif
									   )
		: host_(host)
		, port_(port)
		, max_retries_(max_retries)
		, initial_backoff_(initial_backoff)
#ifdef WITH_COMMON_SYSTEM
		, circuit_breaker_(std::make_unique<common::resilience::circuit_breaker>(cb_config))
#endif
	{
		client_ = std::make_shared<core::messaging_client>(client_id);
#ifdef WITH_COMMON_SYSTEM
		NETWORK_LOG_INFO("[resilient_client] Created with max_retries=" +
			std::to_string(max_retries) + ", initial_backoff=" +
			std::to_string(initial_backoff.count()) + "ms" +
			", circuit_breaker failure_threshold=" +
			std::to_string(cb_config.failure_threshold));
#else
		NETWORK_LOG_INFO("[resilient_client] Created with max_retries=" +
			std::to_string(max_retries) + ", initial_backoff=" +
			std::to_string(initial_backoff.count()) + "ms");
#endif
	}

	resilient_client::~resilient_client() noexcept
	{
		try
		{
			if (is_connected_.load())
			{
				(void)disconnect();
			}
		}
		catch (...)
		{
			// Destructor must not throw
		}
	}

	auto resilient_client::connect() -> VoidResult
	{
		if (is_connected_.load())
		{
			return ok();
		}

		for (size_t attempt = 1; attempt <= max_retries_; ++attempt)
		{
			NETWORK_LOG_INFO("[resilient_client] Connection attempt " +
				std::to_string(attempt) + "/" + std::to_string(max_retries_));

			// Invoke reconnect callback if set
			if (reconnect_callback_)
			{
				reconnect_callback_(attempt);
			}

			// Attempt to connect
			auto result = client_->start_client(host_, port_);
			if (!result.is_err())
			{
				is_connected_.store(true);
				NETWORK_LOG_INFO("[resilient_client] Connected successfully on attempt " +
					std::to_string(attempt));
				return ok();
			}

			NETWORK_LOG_WARN("[resilient_client] Connection attempt " +
				std::to_string(attempt) + " failed: " + result.error().message);

			// Don't sleep after last failed attempt
			if (attempt < max_retries_)
			{
				auto backoff = calculate_backoff(attempt);
				NETWORK_LOG_INFO("[resilient_client] Backing off for " +
					std::to_string(backoff.count()) + "ms");
				std::this_thread::sleep_for(backoff);
			}
		}

		return error_void(
			error_codes::network_system::connection_failed,
			"Failed to connect after " + std::to_string(max_retries_) + " attempts",
			"resilient_client::connect",
			"Host: " + host_ + ", Port: " + std::to_string(port_)
		);
	}

	auto resilient_client::disconnect() -> VoidResult
	{
		if (!is_connected_.load())
		{
			return ok();
		}

		auto result = client_->stop_client();
		if (!result.is_err())
		{
			is_connected_.store(false);

			// Invoke disconnect callback if set
			if (disconnect_callback_)
			{
				disconnect_callback_();
			}

			NETWORK_LOG_INFO("[resilient_client] Disconnected successfully");
		}
		else
		{
			NETWORK_LOG_ERROR("[resilient_client] Failed to disconnect: " +
				result.error().message);
		}

		return result;
	}

	auto resilient_client::send_with_retry(std::vector<uint8_t>&& data) -> VoidResult
	{
#ifdef WITH_COMMON_SYSTEM
		// Check circuit breaker first
		if (!circuit_breaker_->allow_request())
		{
			NETWORK_LOG_WARN("[resilient_client] Circuit breaker is open, failing fast");
			return error_void(
				kcenon::network::error_codes_ext::network_system::circuit_open,
				"Circuit breaker is open",
				"resilient_client::send_with_retry",
				"Circuit state: " + common::resilience::to_string(circuit_breaker_->get_state())
			);
		}
#endif

		// Keep a copy of the data for retries
		auto data_copy = data;

		for (size_t attempt = 1; attempt <= max_retries_; ++attempt)
		{
			// Check if connected
			if (!is_connected_.load() || !client_->is_connected())
			{
				NETWORK_LOG_WARN("[resilient_client] Not connected, attempting reconnection");

				// Attempt to reconnect
				auto reconnect_result = reconnect();
				if (reconnect_result.is_err())
				{
					NETWORK_LOG_ERROR("[resilient_client] Reconnection failed: " +
						reconnect_result.error().message);

#ifdef WITH_COMMON_SYSTEM
					// Record failure to circuit breaker
					circuit_breaker_->record_failure();
#endif

					// Exponential backoff before next retry
					if (attempt < max_retries_)
					{
						auto backoff = calculate_backoff(attempt);
						NETWORK_LOG_INFO("[resilient_client] Backing off for " +
							std::to_string(backoff.count()) + "ms");
						std::this_thread::sleep_for(backoff);
					}
					continue;
				}
			}

			// Try to send
			std::vector<uint8_t> send_data = data_copy;  // Make a copy for this attempt
			auto result = client_->send_packet(std::move(send_data));

			if (!result.is_err())
			{
#ifdef WITH_COMMON_SYSTEM
				// Record success to circuit breaker
				circuit_breaker_->record_success();
#endif

				NETWORK_LOG_DEBUG("[resilient_client] Sent " +
					std::to_string(data_copy.size()) + " bytes successfully");
				return ok();
			}

			NETWORK_LOG_WARN("[resilient_client] Send attempt " +
				std::to_string(attempt) + " failed: " + result.error().message);

#ifdef WITH_COMMON_SYSTEM
			// Record failure to circuit breaker
			circuit_breaker_->record_failure();
#endif

			// Mark as disconnected if send failed
			is_connected_.store(false);

			// Disconnect the client
			(void)client_->stop_client();

			// Invoke disconnect callback if set
			if (disconnect_callback_)
			{
				disconnect_callback_();
			}

			// Don't sleep after last failed attempt
			if (attempt < max_retries_)
			{
				auto backoff = calculate_backoff(attempt);
				NETWORK_LOG_INFO("[resilient_client] Backing off for " +
					std::to_string(backoff.count()) + "ms");
				std::this_thread::sleep_for(backoff);
			}
		}

		return error_void(
			error_codes::network_system::send_failed,
			"Failed to send after " + std::to_string(max_retries_) + " attempts",
			"resilient_client::send_with_retry",
			"Data size: " + std::to_string(data_copy.size()) + " bytes"
		);
	}

	auto resilient_client::is_connected() const noexcept -> bool
	{
		return is_connected_.load() && client_->is_connected();
	}

	auto resilient_client::set_reconnect_callback(
		std::function<void(size_t)> callback) -> void
	{
		reconnect_callback_ = std::move(callback);
	}

	auto resilient_client::set_disconnect_callback(
		std::function<void()> callback) -> void
	{
		disconnect_callback_ = std::move(callback);
	}

	auto resilient_client::get_client() const
		-> std::shared_ptr<core::messaging_client>
	{
		return client_;
	}

	auto resilient_client::reconnect() -> VoidResult
	{
		NETWORK_LOG_INFO("[resilient_client] Attempting reconnection");

		// Ensure client is disconnected first
		if (client_->is_connected())
		{
			(void)client_->stop_client();
		}

		// Try to reconnect with retry logic
		for (size_t attempt = 1; attempt <= max_retries_; ++attempt)
		{
			if (reconnect_callback_)
			{
				reconnect_callback_(attempt);
			}

			auto result = client_->start_client(host_, port_);
			if (!result.is_err())
			{
				is_connected_.store(true);
				NETWORK_LOG_INFO("[resilient_client] Reconnected successfully on attempt " +
					std::to_string(attempt));
				return ok();
			}

			NETWORK_LOG_WARN("[resilient_client] Reconnection attempt " +
				std::to_string(attempt) + " failed: " + result.error().message);

			if (attempt < max_retries_)
			{
				auto backoff = calculate_backoff(attempt);
				std::this_thread::sleep_for(backoff);
			}
		}

		return error_void(
			error_codes::network_system::connection_failed,
			"Failed to reconnect after " + std::to_string(max_retries_) + " attempts",
			"resilient_client::reconnect",
			"Host: " + host_ + ", Port: " + std::to_string(port_)
		);
	}

	auto resilient_client::calculate_backoff(size_t attempt) const
		-> std::chrono::milliseconds
	{
		// Exponential backoff: initial_backoff * 2^(attempt-1)
		// Capped at 30 seconds to prevent excessive delays
		auto backoff = initial_backoff_ * (1 << (attempt - 1));
		auto max_backoff = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::seconds(30));

		return std::min(backoff, max_backoff);
	}

#ifdef WITH_COMMON_SYSTEM
	auto resilient_client::circuit_state() const -> common::resilience::circuit_state
	{
		return circuit_breaker_->get_state();
	}
#endif

} // namespace kcenon::network::utils
