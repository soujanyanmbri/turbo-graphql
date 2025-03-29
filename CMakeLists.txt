cmake_minimum_required(VERSION 3.16)
project(graphql_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512)
check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
check_cxx_compiler_flag("-mavx512bw" COMPILER_SUPPORTS_AVX512BW)

if (COMPILER_SUPPORTS_AVX512)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f")
elseif (COMPILER_SUPPORTS_AVX512BW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512bw")
elseif (COMPILER_SUPPORTS_AVX2)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
elseif (COMPILER_SUPPORTS_SSE42)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
add_library(graphql_core ${SOURCES})
add_executable(graphql_parser src/main.cpp)
target_link_libraries(graphql_parser PRIVATE graphql_core)
enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP true
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Create test executable
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
add_executable(graphql_tests ${TEST_SOURCES})
target_link_libraries(graphql_tests PRIVATE graphql_core gtest gtest_main)

# Register tests
add_test(NAME GraphQLParserTests COMMAND graphql_tests)
