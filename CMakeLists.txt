cmake_minimum_required(VERSION 3.16)
project(graphql_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable optimization flags (but DON'T force SIMD globally)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512)
check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
check_cxx_compiler_flag("-mavx512bw" COMPILER_SUPPORTS_AVX512BW)

# We'll apply SIMD flags to specific files, not globally

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

# Add parser sources explicitly
set(PARSER_SOURCES
    src/parser/parser.cpp
)

add_library(graphql_core ${SOURCES})

# Apply SIMD flags only to SIMD implementation files
if (COMPILER_SUPPORTS_AVX2)
    set_source_files_properties(
        src/simd/impl/avx2_impl.cpp
        src/lexer/lexer.cpp
        PROPERTIES COMPILE_FLAGS "-mavx2"
    )
endif()

if (COMPILER_SUPPORTS_SSE42)
    set_source_files_properties(
        src/simd/impl/sse_impl.cpp
        PROPERTIES COMPILE_FLAGS "-msse4.2"
    )
endif()

add_executable(graphql_parser src/main.cpp)
target_link_libraries(graphql_parser PRIVATE graphql_core)

# Add benchmark executables
add_executable(graphql_benchmark benchmark.cpp)
target_link_libraries(graphql_benchmark PRIVATE graphql_core)

add_executable(benchmark benchmark_simd_simple.cpp)
target_link_libraries(benchmark PRIVATE graphql_core)

add_executable(benchmark_comparison benchmark_comparison.cpp)
target_link_libraries(benchmark_comparison PRIVATE graphql_core)

enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Create test executable
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
add_executable(graphql_tests ${TEST_SOURCES})
target_link_libraries(graphql_tests PRIVATE graphql_core gtest gtest_main)

# Register tests
add_test(NAME GraphQLParserTests COMMAND graphql_tests)
