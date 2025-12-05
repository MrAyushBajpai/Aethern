cmake_minimum_required(VERSION 3.12)
project(backend)

set(CMAKE_CXX_STANDARD 17)

# Find libsodium (vcpkg usually)
find_package(unofficial-sodium CONFIG REQUIRED)

#
# === CORE LIBRARY ===
#
add_library(core
    src/core/Item.cpp
    src/core/Scheduler.cpp
)

target_include_directories(core PUBLIC src)

#
# === AUTH LIBRARY ===
#
add_library(auth
    src/auth/User.cpp
    src/auth/AuthManager.cpp
)

target_include_directories(auth PUBLIC src)

target_link_libraries(auth PRIVATE unofficial-sodium::sodium)

#
# === STORAGE LIBRARY ===
#
add_library(storage
    src/storage/Storage.cpp
)

target_include_directories(storage PUBLIC src)
target_link_libraries(storage PRIVATE unofficial-sodium::sodium)


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
)
