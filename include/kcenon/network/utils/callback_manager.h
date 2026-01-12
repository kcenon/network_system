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

#include <cstdint>
#include <functional>
#include <mutex>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace kcenon::network::utils
{

	/*!
	 * \class callback_manager
	 * \brief Thread-safe callback storage and invocation utility.
	 *
	 * This template class provides a centralized way to store and invoke
	 * multiple callback types with thread safety. It replaces the repeated
	 * callback handling code in CRTP base classes.
	 *
	 * \tparam CallbackTypes The callback function types to manage.
	 *
	 * ### Thread Safety
	 * - Setting callbacks is protected by mutex
	 * - Invocation copies the callback under lock, then invokes outside lock
	 * - This prevents deadlocks when callbacks trigger other operations
	 *
	 * ### Usage Example
	 * \code
	 * using my_callbacks = callback_manager<
	 *     std::function<void(std::vector<uint8_t>&&)>,  // receive
	 *     std::function<void()>,                        // connected
	 *     std::function<void()>                         // disconnected
	 * >;
	 *
	 * my_callbacks callbacks;
	 * callbacks.set<0>([](auto&& data) { process(data); });
	 * callbacks.invoke<0>(std::move(data));
	 * \endcode
	 */
	template<typename... CallbackTypes>
	class callback_manager
	{
	public:
		/*!
		 * \brief Default constructor.
		 */
		callback_manager() = default;

		/*!
		 * \brief Destructor.
		 */
		~callback_manager() = default;

		// Non-copyable, movable
		callback_manager(const callback_manager&) = delete;
		callback_manager& operator=(const callback_manager&) = delete;
		callback_manager(callback_manager&&) noexcept = default;
		callback_manager& operator=(callback_manager&&) noexcept = default;

		/*!
		 * \brief Sets a callback at the specified index.
		 * \tparam Index The index of the callback type.
		 * \param callback The callback function to store.
		 */
		template<std::size_t Index>
		auto set(std::tuple_element_t<Index, std::tuple<CallbackTypes...>> callback) -> void
		{
			std::lock_guard lock(mutex_);
			std::get<Index>(callbacks_) = std::move(callback);
		}

		/*!
		 * \brief Sets a callback by type.
		 * \tparam T The callback type (must be unique in CallbackTypes).
		 * \param callback The callback function to store.
		 */
		template<typename T>
		auto set(T callback) -> void
		{
			std::lock_guard lock(mutex_);
			std::get<T>(callbacks_) = std::move(callback);
		}

		/*!
		 * \brief Gets a callback at the specified index.
		 * \tparam Index The index of the callback type.
		 * \return The callback function (may be empty).
		 */
		template<std::size_t Index>
		[[nodiscard]] auto get() const -> std::tuple_element_t<Index, std::tuple<CallbackTypes...>>
		{
			std::lock_guard lock(mutex_);
			return std::get<Index>(callbacks_);
		}

		/*!
		 * \brief Gets a callback by type.
		 * \tparam T The callback type.
		 * \return The callback function (may be empty).
		 */
		template<typename T>
		[[nodiscard]] auto get() const -> T
		{
			std::lock_guard lock(mutex_);
			return std::get<T>(callbacks_);
		}

		/*!
		 * \brief Invokes a callback at the specified index if set.
		 * \tparam Index The index of the callback type.
		 * \tparam Args The argument types.
		 * \param args The arguments to pass to the callback.
		 *
		 * ### Thread Safety
		 * The callback is copied under lock, then invoked without lock.
		 * This prevents deadlocks if the callback triggers set() calls.
		 */
		template<std::size_t Index, typename... Args>
		auto invoke(Args&&... args) -> void
		{
			auto callback = get<Index>();
			if (callback)
			{
				callback(std::forward<Args>(args)...);
			}
		}

		/*!
		 * \brief Invokes a callback by type if set.
		 * \tparam T The callback type.
		 * \tparam Args The argument types.
		 * \param args The arguments to pass to the callback.
		 */
		template<typename T, typename... Args>
		auto invoke(Args&&... args) -> void
		{
			auto callback = get<T>();
			if (callback)
			{
				callback(std::forward<Args>(args)...);
			}
		}

		/*!
		 * \brief Invokes a callback conditionally.
		 * \tparam Index The index of the callback type.
		 * \tparam Args The argument types.
		 * \param condition If false, the callback is not invoked.
		 * \param args The arguments to pass to the callback.
		 */
		template<std::size_t Index, typename... Args>
		auto invoke_if(bool condition, Args&&... args) -> void
		{
			if (condition)
			{
				invoke<Index>(std::forward<Args>(args)...);
			}
		}

		/*!
		 * \brief Clears all callbacks.
		 */
		auto clear() -> void
		{
			std::lock_guard lock(mutex_);
			callbacks_ = std::tuple<CallbackTypes...>{};
		}

		/*!
		 * \brief Clears a specific callback.
		 * \tparam Index The index of the callback to clear.
		 */
		template<std::size_t Index>
		auto clear() -> void
		{
			std::lock_guard lock(mutex_);
			std::get<Index>(callbacks_) = {};
		}

	private:
		mutable std::mutex mutex_;
		std::tuple<CallbackTypes...> callbacks_;
	};

	// Convenience type aliases for common callback configurations

	/*!
	 * \brief Callback manager for TCP client callbacks.
	 *
	 * Index mapping:
	 * - 0: receive_callback (vector<uint8_t>&&)
	 * - 1: connected_callback ()
	 * - 2: disconnected_callback ()
	 * - 3: error_callback (error_code)
	 */
	using tcp_client_callbacks = callback_manager<
	    std::function<void(const std::vector<uint8_t>&)>, // receive
	    std::function<void()>,                            // connected
	    std::function<void()>,                            // disconnected
	    std::function<void(std::error_code)>              // error
	    >;

	/*!
	 * \brief Callback indices for tcp_client_callbacks.
	 */
	struct tcp_client_callback_index
	{
		static constexpr std::size_t receive = 0;
		static constexpr std::size_t connected = 1;
		static constexpr std::size_t disconnected = 2;
		static constexpr std::size_t error = 3;
	};

} // namespace kcenon::network::utils
