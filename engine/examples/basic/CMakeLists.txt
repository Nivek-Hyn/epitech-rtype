cmake_minimum_required(VERSION 3.24)

project(RTypeEngineExample LANGUAGES CXX)

find_package(RTypeEngine 0.1 CONFIG REQUIRED COMPONENTS ECS Systems)

add_executable(engine_example main.cpp)
target_link_libraries(engine_example PRIVATE RTypeEngine::Systems)
set_target_properties(engine_example PROPERTIES CXX_EXTENSIONS OFF)

enable_testing()
add_test(NAME EngineExample.ComponentsAndSystems COMMAND engine_example)
