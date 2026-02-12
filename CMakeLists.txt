cmake_minimum_required(VERSION 3.20)
project(AVX2_Packet_Filter VERSION 1.0 LANGUAGES CXX)

# --- Compiler Optimization ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -O3: Max speed
# -march=native: Unlock AVX2 instructions on your i9-14900K
add_compile_options(-O3 -march=native -Wall -Wextra -Wpedantic)

#Include Headers
add_library(avx2_filter INTERFACE)
target_include_directories(avx2_filter INTERFACE include)

#Benchmark
find_package(benchmark REQUIRED)

add_executable(run_benchmarks benchmarks/filter_bench.cpp)
target_link_libraries(run_benchmarks PRIVATE avx2_filter benchmark::benchmark pthread)

#Demo Server Using IO_URING
find_package(PkgConfig REQUIRED)
pkg_check_modules(URING REQUIRED liburing)

add_executable(demo_server 
    examples/demo_server.cpp 
    examples/server_impl.cpp
)
target_link_libraries(demo_server PRIVATE avx2_filter ${URING_LIBRARIES})
target_include_directories(demo_server PRIVATE examples)