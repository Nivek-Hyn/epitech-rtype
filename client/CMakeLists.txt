add_executable(r-type_client
    src/main.cpp
    src/ResourceManager.cpp
    src/game/LocalServer.cpp
    src/game/Session.cpp
    src/game/SessionHud.cpp
    src/render/EntityRenderer.cpp
    src/render/WorldBackground.cpp
    src/net/AsioNetworkClient.cpp
    src/ui/Starfield.cpp
    src/ui/Button.cpp
    src/ui/TextInput.cpp
    src/ui/OptionSelector.cpp
    src/ui/GameConfig.cpp
    src/ui/MainMenu.cpp
)
target_include_directories(r-type_client PRIVATE src)
target_link_libraries(r-type_client PRIVATE rtype_server_lib rtype_common rtype_engine asio nlohmann_json::nlohmann_json SFML::Graphics SFML::Audio)
if(NOT MSVC)
    # GNU ld reads static libraries once: FreeType and HarfBuzz depend on each other.
    target_link_libraries(r-type_client PRIVATE -Wl,--start-group freetype harfbuzz -Wl,--end-group)
endif()
rtype_set_warnings(r-type_client)
