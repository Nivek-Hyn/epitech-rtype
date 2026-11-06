cmake_minimum_required(VERSION 3.24)

project(RType VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(RTYPE_BUILD_CLIENT "Build the graphical client" ON)
option(RTYPE_BUILD_TESTS "Build unit tests" ON)

# Binaries land at the repository root, as required by the subject.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

include(cmake/Dependencies.cmake)
include(cmake/Warnings.cmake)

add_subdirectory(engine)
add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(tools/level-editor)
add_subdirectory(games/brickrush)

if(RTYPE_BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(RTYPE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(cmake/InstallGame.cmake)
include(cmake/PackageGame.cmake)
