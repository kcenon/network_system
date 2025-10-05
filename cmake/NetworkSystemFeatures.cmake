##################################################
# NetworkSystemFeatures.cmake
#
# Feature detection module for NetworkSystem
# Checks ASIO and coroutine support
##################################################

include(CheckCXXSourceCompiles)

##################################################
# Check ASIO support
##################################################
function(check_asio_support)
    set(_prev_required_includes "${CMAKE_REQUIRED_INCLUDES}")
    set(_prev_required_definitions "${CMAKE_REQUIRED_DEFINITIONS}")

    if(ASIO_INCLUDE_DIR)
        set(CMAKE_REQUIRED_INCLUDES "${ASIO_INCLUDE_DIR}")
        if(NOT USE_BOOST_ASIO)
            set(CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS} -DASIO_STANDALONE -DASIO_NO_DEPRECATED")
        endif()
    elseif(USE_BOOST_ASIO AND Boost_INCLUDE_DIRS)
        set(CMAKE_REQUIRED_INCLUDES "${Boost_INCLUDE_DIRS}")
    endif()

    # Check for basic ASIO functionality
    check_cxx_source_compiles("
        #ifdef USE_BOOST_ASIO
        #include <boost/asio.hpp>
        #else
        #include <asio.hpp>
        #endif

        int main() {
            #ifdef USE_BOOST_ASIO
            boost::asio::io_context io;
            #else
            asio::io_context io;
            #endif
            return 0;
        }
    " HAS_ASIO_BASIC)

    if(HAS_ASIO_BASIC)
        message(STATUS "✅ ASIO basic functionality verified")
    else()
        message(WARNING "ASIO basic functionality check failed")
    endif()

    set(CMAKE_REQUIRED_INCLUDES "${_prev_required_includes}")
    set(CMAKE_REQUIRED_DEFINITIONS "${_prev_required_definitions}")

    set(HAS_ASIO_BASIC ${HAS_ASIO_BASIC} PARENT_SCOPE)
endfunction()

##################################################
# Check coroutine support
##################################################
function(check_coroutine_support)
    # Check for C++20 coroutines
    check_cxx_source_compiles("
        #include <coroutine>

        struct promise;
        struct coroutine : std::coroutine_handle<promise> {
            using promise_type = ::promise;
        };

        struct promise {
            coroutine get_return_object() { return {coroutine::from_promise(*this)}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() {}
        };

        coroutine test_coroutine() {
            co_return;
        }

        int main() {
            auto c = test_coroutine();
            return 0;
        }
    " HAS_STD_COROUTINE)

    if(HAS_STD_COROUTINE)
        add_definitions(-DHAS_STD_COROUTINE)
        message(STATUS "✅ C++20 coroutines supported")
    else()
        message(STATUS "C++20 coroutines not supported - using callback-based approach")
    endif()

    set(HAS_STD_COROUTINE ${HAS_STD_COROUTINE} PARENT_SCOPE)
endfunction()

##################################################
# Main feature check function
##################################################
function(check_network_system_features)
    message(STATUS "Checking NetworkSystem feature support...")

    check_asio_support()
    check_coroutine_support()

    message(STATUS "Feature detection complete")
endfunction()
