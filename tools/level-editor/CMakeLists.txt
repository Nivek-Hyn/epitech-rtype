# Level editor: reads and writes assets/levels/*.json. Uses the engine for its preview, not rtype_common.

# Level files and preview, without SFML, so the unit tests can use them.
add_library(level_editor_core STATIC src/Level.cpp src/Preview.cpp)
target_include_directories(level_editor_core PUBLIC include)
target_link_libraries(level_editor_core PUBLIC RTypeEngine::ECS RTypeEngine::Systems nlohmann_json::nlohmann_json)
rtype_set_warnings(level_editor_core)

if(RTYPE_BUILD_CLIENT)
    add_executable(r-type_level_editor src/main.cpp)
    target_link_libraries(r-type_level_editor PRIVATE level_editor_core RTypeEngine::Core SFML::Graphics
        -Wl,--start-group freetype harfbuzz -Wl,--end-group)
    rtype_set_warnings(r-type_level_editor)
endif()
