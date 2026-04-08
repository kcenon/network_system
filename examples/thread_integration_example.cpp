/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file thread_integration_example.cpp
 * @brief Demonstrates thread pool integration for async network operations.
 * @example thread_integration_example.cpp
 *
 * @par Category
 * Integration - Thread pool management
 *
 * Demonstrates:
 * - basic_thread_pool creation and task submission
 * - thread_integration_manager singleton
 * - Delayed task scheduling
 * - Pool metrics
 *
 * @see kcenon::network::integration::basic_thread_pool
 * @see kcenon::network::integration::thread_integration_manager
 */

#include <kcenon/network/integration/thread_integration.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace kcenon::network;

int main()
{
	std::cout << "=== Thread Integration Example ===" << std::endl;

	// 1. Create a thread pool
	std::cout << "\n1. Creating thread pool (4 workers):" << std::endl;
	auto pool = std::make_shared<integration::basic_thread_pool>(4);
	std::cout << "   Worker count: " << pool->worker_count() << std::endl;
	std::cout << "   Running: " << (pool->is_running() ? "yes" : "no") << std::endl;

	// 2. Submit tasks
	std::cout << "\n2. Submitting tasks:" << std::endl;
	std::atomic<int> completed{0};

	std::vector<std::future<void>> futures;
	for (int i = 0; i < 5; ++i)
	{
		auto future = pool->submit(
			[i, &completed]()
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				completed.fetch_add(1);
				std::cout << "   Task " << i << " completed on thread "
						  << std::this_thread::get_id() << std::endl;
			});
		futures.push_back(std::move(future));
	}

	// Wait for all tasks
	for (auto& f : futures)
	{
		f.get();
	}
	std::cout << "   All " << completed.load() << " tasks completed" << std::endl;

	// 3. Register with global manager
	std::cout << "\n3. Thread integration manager:" << std::endl;
	auto& manager = integration::thread_integration_manager::instance();
	manager.set_thread_pool(pool);

	auto global_pool = manager.get_thread_pool();
	std::cout << "   Global pool set: " << (global_pool != nullptr ? "yes" : "no") << std::endl;

	// Submit via manager
	auto task_future = manager.submit_task(
		[]() { std::cout << "   Task submitted via global manager" << std::endl; });
	task_future.get();

	// 4. Check metrics
	std::cout << "\n4. Pool metrics:" << std::endl;
	std::cout << "   Workers: " << pool->worker_count() << std::endl;
	std::cout << "   Pending: " << pool->pending_tasks() << std::endl;
	std::cout << "   Completed: " << pool->completed_tasks() << std::endl;

	// 5. Cleanup
	std::cout << "\n5. Stopping pool..." << std::endl;
	pool->stop(true); // wait for pending tasks
	std::cout << "   Running: " << (pool->is_running() ? "yes" : "no") << std::endl;

	std::cout << "\nDone." << std::endl;
	return 0;
}
