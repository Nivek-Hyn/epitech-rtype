# Reads and writes the level files (assets/levels/*.json) for the level editor of the client.
# No SFML here, so the unit tests can use it.
add_library(level_editor_core STATIC src/Level.cpp)
target_include_directories(level_editor_core PUBLIC include)
target_link_libraries(level_editor_core PUBLIC nlohmann_json::nlohmann_json)
rtype_set_warnings(level_editor_core)
