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
elseif (COMPILER_SUPPORTS_AVX2)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
elseif (COMPILER_SUPPORTS_SSE42)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
elseif (COMPILER_SUPPORTS_AVX512BW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512bw")
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

# Download and configure Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Main executable sources (excluding test files)
file(GLOB_RECURSE SOURCES "src/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*_test\.cpp$")

# Create main executable
add_executable(graphql_parser ${SOURCES})

# Test executable
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
add_executable(graphql_parser_tests ${TEST_SOURCES} ${SOURCES})
target_link_libraries(graphql_parser_tests GTest::gtest_main GTest::gmock)

# Place the compiled executables in the main project folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# Register tests
include(GoogleTest)
gtest_discover_tests(graphql_parser_tests)
