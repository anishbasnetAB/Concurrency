cmake_minimum_required(VERSION 3.15)
project(Concurrency CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# Release builds get -O2 unless the user passes something else.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ── Thread Pool ──────────────────────────────────────────────────────────────
add_executable(thread_pool_demo thread_pool/demo.cpp)
target_include_directories(thread_pool_demo PRIVATE thread_pool)
target_link_libraries(thread_pool_demo Threads::Threads)

# ── SPSC Queue ───────────────────────────────────────────────────────────────
add_executable(spsc_demo spsc/demo.cpp)
target_include_directories(spsc_demo PRIVATE spsc)
target_link_libraries(spsc_demo Threads::Threads)

# ── Lock-Free Stack ──────────────────────────────────────────────────────────
add_executable(lock_free_demo lock_free/demo.cpp)
target_include_directories(lock_free_demo PRIVATE lock_free)
target_link_libraries(lock_free_demo Threads::Threads)

# ── Memory Ordering Examples ─────────────────────────────────────────────────
add_executable(memory_ordering memory_ordering/examples.cpp)
target_link_libraries(memory_ordering Threads::Threads)

# ── Benchmark Suite ───────────────────────────────────────────────────────────
add_executable(bench benchmarks/bench.cpp)
target_include_directories(bench PRIVATE spsc lock_free thread_pool)
target_link_libraries(bench Threads::Threads)
