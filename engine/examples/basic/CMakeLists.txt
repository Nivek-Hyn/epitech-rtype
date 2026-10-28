cmake_minimum_required(VERSION 3.24)

project(RTypeEngineExample LANGUAGES CXX)

find_package(RTypeEngine 0.1 CONFIG REQUIRED)

add_executable(engine_example main.cpp)
target_link_libraries(engine_example PRIVATE RTypeEngine::Engine)
set_target_properties(engine_example PROPERTIES CXX_EXTENSIONS OFF)

enable_testing()
add_test(NAME EngineExample.Movement COMMAND engine_example)
