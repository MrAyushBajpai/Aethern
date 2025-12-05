cmake_minimum_required(VERSION 3.14)
project(backend)

set(CMAKE_CXX_STANDARD 17)

if (MSVC)
    add_compile_options(/utf-8 /D_UNICODE /DUNICODE)
endif()

# Find libsodium (vcpkg usually)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(spdlog REQUIRED)

#
# === CORE LIBRARY ===
#
add_library(core
    src/core/Item.cpp
    src/core/Scheduler.cpp
)

target_include_directories(core PUBLIC src)
target_link_libraries(core PRIVATE spdlog::spdlog)

#
# === AUTH LIBRARY ===
#
add_library(auth
    src/auth/User.cpp
    src/auth/AuthManager.cpp
)

target_include_directories(auth PUBLIC src)

target_link_libraries(auth PRIVATE unofficial-sodium::sodium spdlog::spdlog)

#
# === STORAGE LIBRARY ===
#
add_library(storage
    src/storage/Storage.cpp
)

target_include_directories(storage PUBLIC src)
target_link_libraries(storage PRIVATE unofficial-sodium::sodium spdlog::spdlog)


#
# === CLI EXECUTABLE ===
#
add_executable(cli
    src/cli/main.cpp
)

# Link everything needed
target_link_libraries(cli
    PRIVATE
        core
        auth
        storage
        unofficial-sodium::sodium
        spdlog::spdlog
)
