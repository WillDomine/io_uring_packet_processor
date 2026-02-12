cmake_minimum_required(VERSION 3.20)
project(PacketEngine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -O3: Max optimization
# -march=native: Use instructions specific to i9-14900K (AVX2, etc.)
# -Wall -Wextra: Show all warnings (crucial for C++)
add_compile_options(-O3 -march=native -Wall -Wextra -Wpedantic)

# Check for liburing
find_package(PkgConfig REQUIRED)
pkg_check_modules(URING REQUIRED liburing)

add_executable(packet_engine src/main.cpp src/server.cpp)

target_link_libraries(packet_engine PRIVATE ${URING_LIBRARIES})
target_include_directories(packet_engine PRIVATE ${URING_INCLUDE_DIRS} include)

find_package(benchmark REQUIRED)

add_executable(run_benchmarks benchmarks/filter_bench.cpp)

target_link_libraries(run_benchmarks PRIVATE benchmark::benchmark pthread)

target_compile_options(run_benchmarks PRIVATE -O3 -march=native)
