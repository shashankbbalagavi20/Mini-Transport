cmake_minimum_required(VERSION 3.14)
project(MiniTransport VERSION 1.0.0 LANGUAGES CXX)

# Enforce C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release optimization if no build type is specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Core transport library (Header-only interface library for protocol.hpp)
add_library(transport_core INTERFACE)
target_include_directories(transport_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Production Binaries
add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE transport_core)

add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client PRIVATE transport_core)

# Testing Configuration
enable_testing()
find_package(GTest REQUIRED)

add_executable(test_protocol tests/protocol_tests.cpp)
target_link_libraries(test_protocol 
    PRIVATE 
        transport_core 
        GTest::gtest 
        GTest::gtest_main 
        pthread
)

include(GoogleTest)
gtest_discover_tests(test_protocol)