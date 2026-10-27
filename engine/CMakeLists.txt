cmake_minimum_required(VERSION 3.24)

project(RTypeEngine VERSION 0.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Warnings.cmake)

# Generic game engine: knows nothing about R-Type. Never include anything from common/, server/ or client/ here.
add_library(rtype_engine STATIC
    src/Registry.cpp
    src/FixedTimestep.cpp
)
target_include_directories(rtype_engine PUBLIC include)
target_compile_features(rtype_engine PUBLIC cxx_std_20)
set_target_properties(rtype_engine PROPERTIES CXX_EXTENSIONS OFF)
rtype_set_warnings(rtype_engine)
