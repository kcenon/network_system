##################################################
# network_system_summary.cmake
#
# Final configuration printout. Included at the very end of
# the top-level CMakeLists.txt so all option values are
# resolved and all subdirectories have already added their
# logging.
##################################################

message(STATUS "========================================")
message(STATUS "network_system Build Configuration:")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Dependency Chain (Tier 4):")
message(STATUS "  network_system")
message(STATUS "    ├── common_system (Tier 0): ${BUILD_WITH_COMMON_SYSTEM} [REQUIRED - provides ILogger, EventBus]")
message(STATUS "    ├── thread_system (Tier 1): ${BUILD_WITH_THREAD_SYSTEM} [REQUIRED]")
message(STATUS "    ├── logger_system (Tier 2): ${BUILD_WITH_LOGGER_SYSTEM} [OPTIONAL - runtime binding]")
message(STATUS "    ├── container_system (Tier 1): ${BUILD_WITH_CONTAINER_SYSTEM}")
message(STATUS "    └── monitoring_system (Tier 3): EventBus [NO COMPILE-TIME DEP]")
message(STATUS "")
message(STATUS "Monitoring Mode: EventBus-based metric publishing")
message(STATUS "  - Metrics published via common_system's EventBus")
message(STATUS "  - External consumers subscribe to network_metric_event")
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "  Integration tests: ${NETWORK_BUILD_INTEGRATION_TESTS}")
message(STATUS "  Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Verify build: ${BUILD_VERIFY_BUILD}")
message(STATUS "  TLS/SSL support: ${BUILD_TLS_SUPPORT}")
message(STATUS "  WebSocket support: ${BUILD_WEBSOCKET_SUPPORT}")
message(STATUS "  Messaging bridge: ${BUILD_MESSAGING_BRIDGE}")
message(STATUS "  Benchmarks: ${NETWORK_BUILD_BENCHMARKS}")
message(STATUS "  Official gRPC: ${NETWORK_ENABLE_GRPC_OFFICIAL}")
message(STATUS "  C++20 Modules: ${NETWORK_BUILD_MODULES}")
message(STATUS "")
message(STATUS "Sanitizers:")
message(STATUS "  AddressSanitizer: ${ENABLE_ASAN}")
message(STATUS "  ThreadSanitizer: ${ENABLE_TSAN}")
message(STATUS "  UndefinedBehaviorSanitizer: ${ENABLE_UBSAN}")
message(STATUS "  Coverage: ${ENABLE_COVERAGE}")
message(STATUS "========================================")
