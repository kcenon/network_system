// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/network/detail/session/messaging_session.h"
#include "kcenon/network/internal/send_coroutine.h" // for async_send_with_pipeline_co / no_co
#include "kcenon/network/integration/logger_integration.h"
#include <string_view>
#include <type_traits>

// Use nested namespace definition (C++17)
namespace kcenon::network::session
{

	// Use string_view in constructor for efficiency (C++17)
	messaging_session::messaging_session(asio::ip::tcp::socket socket,
										 std::string_view server_id)
		: server_id_(server_id)
	{
		// Create the tcp_socket wrapper
		socket_ = std::make_shared<tcp_socket>(std::move(socket));

		// Initialize the pipeline (stub)
		pipeline_ = make_default_pipeline();

		// Default modes - could use inline initialization in header with C++17
		compress_mode_ = false;
		encrypt_mode_ = false;
	}

	messaging_session::~messaging_session() { stop_session(); }

	auto messaging_session::start_session() -> void
	{
		if (is_stopped_.load())
		{
			return;
		}

		// Set up callbacks
		auto self = shared_from_this();
		socket_->set_receive_callback(
			[this, self](const std::vector<uint8_t>& data)
			{ on_receive(data); });
		socket_->set_error_callback([this, self](std::error_code ec)
									{ on_error(ec); });

		// Begin reading
		socket_->start_read();

		NETWORK_LOG_INFO("[messaging_session] Started session on server: " + server_id_);
	}

	auto messaging_session::stop_session() -> void
	{
		if (is_stopped_.exchange(true))
		{
			return;
		}
		// Close socket
		if (socket_)
		{
			socket_->socket().close();
		}
		NETWORK_LOG_INFO("[messaging_session] Stopped.");
	}

	auto messaging_session::send_packet(std::vector<uint8_t> data) -> void
	{
		if (is_stopped_.load())
		{
			return;
		}
// Using if constexpr for compile-time branching (C++17)
if constexpr (std::is_same_v<decltype(socket_->socket().get_executor()), asio::io_context::executor_type>)
{
#ifdef USE_STD_COROUTINE
		// Coroutine-based approach
		asio::co_spawn(socket_->socket().get_executor(),
					   async_send_with_pipeline_co(socket_, std::move(data),
												   pipeline_, compress_mode_,
												   encrypt_mode_),
					   [](std::error_code ec)
					   {
						   if (ec)
						   {
							   NETWORK_LOG_ERROR("[messaging_session] Send error: " + ec.message());
						   }
					   });
#else
		// Fallback approach
		auto fut = async_send_with_pipeline_no_co(
			socket_, std::move(data), pipeline_, compress_mode_, encrypt_mode_);
		
		// Use structured binding with try/catch for better error handling (C++17)
		try {
			auto result_ec = fut.get();
			if (result_ec)
			{
			NETWORK_LOG_ERROR("[messaging_session] Send error: " + result_ec.message());
			}
		} catch (const std::exception& e) {
			NETWORK_LOG_ERROR("[messaging_session] Exception while waiting for send: " + std::string(e.what()));
		}
#endif
}
	}

	auto messaging_session::on_receive(const std::vector<uint8_t>& data) -> void
	{
		if (is_stopped_.load())
		{
			return;
		}

		NETWORK_LOG_DEBUG("[messaging_session] Received " + std::to_string(data.size())
				+ " bytes.");

		// Potentially decompress + decrypt
		// e.g. auto uncompressed = pipeline_.decompress(data);
		//      auto decrypted    = pipeline_.decrypt(uncompressed);
		// Then parse or handle the final data
	}

	auto messaging_session::on_error(std::error_code ec) -> void
	{
		NETWORK_LOG_ERROR("[messaging_session] Socket error: " + ec.message());
		stop_session();
	}

} // namespace kcenon::network::session
