cmake_minimum_required(VERSION 3.10)
project(graphql_parser)

set(CMAKE_CXX_STANDARD 17)  # Use C++17 or later

# Source files
file(GLOB_RECURSE SOURCES src/*.cpp)

# Include directories
include_directories(include)

# Create the executable
add_executable(graphql_parser ${SOURCES})

# Enable optimizations
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -pedantic")

# Add unit tests (if using GoogleTest later)
enable_testing()
