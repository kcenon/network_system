/**
 * @file main.cpp
 * @brief Minimal test consumer for kcenon-network-system vcpkg build chain validation
 *
 * Verifies that network_system and its transitive dependencies (common_system,
 * thread_system) can be found, included, and linked via vcpkg. This program
 * validates build chain integrity only — it does not exercise runtime behavior.
 */

#include <cstdlib>
#include <iostream>

// Tier 4: network_system (primary target)
#ifdef HAS_NETWORK_SYSTEM
#include <kcenon/network/types/result.h>
#endif

// Tier 0: common_system (transitive dependency)
#ifdef HAS_COMMON_SYSTEM
#include <kcenon/common/common.h>
#endif

// Tier 1: thread_system (transitive dependency)
#ifdef HAS_THREAD_SYSTEM
#include <kcenon/thread/utils/formatter.h>
#endif

int main()
{
    std::cout << "=== kcenon-network-system vcpkg build chain validation ===" << std::endl;

    int total = 0;
    int found = 0;

    auto check = [&](const char* name, bool available) {
        total++;
        if (available)
        {
            found++;
            std::cout << "  [OK]   " << name << std::endl;
        }
        else
        {
            std::cout << "  [SKIP] " << name << std::endl;
        }
    };

#ifdef HAS_COMMON_SYSTEM
    check("kcenon-common-system  (Tier 0, transitive)", true);
#else
    check("kcenon-common-system  (Tier 0, transitive)", false);
#endif

#ifdef HAS_THREAD_SYSTEM
    check("kcenon-thread-system  (Tier 1, transitive)", true);
#else
    check("kcenon-thread-system  (Tier 1, transitive)", false);
#endif

#ifdef HAS_NETWORK_SYSTEM
    check("kcenon-network-system (Tier 4, primary)",    true);
#else
    check("kcenon-network-system (Tier 4, primary)",    false);
#endif

    std::cout << std::endl;
    std::cout << found << "/" << total << " packages validated." << std::endl;

    if (found == total)
    {
        std::cout << "Build chain validation PASSED." << std::endl;
        return EXIT_SUCCESS;
    }
    else
    {
        std::cout << "Build chain validation PARTIAL." << std::endl;
        return (found > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
