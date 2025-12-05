cmake_minimum_required(VERSION 3.12)
project(spaced_repetition_backend)

set(CMAKE_CXX_STANDARD 17)

# Core library
add_library(core
    src/core/Item.cpp
    src/core/Scheduler.cpp
)

target_include_directories(core PUBLIC src)

# Storage library
add_library(storage
    src/storage/Storage.cpp
)

target_include_directories(storage PUBLIC src)

# CLI executable
add_executable(cli
    src/cli/main.cpp
)

target_link_libraries(cli PRIVATE core storage)
