# Brick Rush: a second game built on the engine only. It never links rtype_common, Asio or R-Type assets.

# The rules, without SFML, so the unit tests can use them.
add_library(brickrush_core STATIC src/Game.cpp src/Collisions.cpp src/Prefabs.cpp)
target_include_directories(brickrush_core PUBLIC include)
target_link_libraries(brickrush_core PUBLIC RTypeEngine::ECS RTypeEngine::Core RTypeEngine::Physics
    RTypeEngine::Systems nlohmann_json::nlohmann_json)
rtype_set_warnings(brickrush_core)

if(RTYPE_BUILD_CLIENT)
    add_executable(brickrush src/main.cpp)
    target_link_libraries(brickrush PRIVATE brickrush_core SFML::Graphics)
    rtype_set_warnings(brickrush)
endif()
