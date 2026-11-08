add_executable(r-type_client
    src/main.cpp
    src/ResourceManager.cpp
    src/game/Session.cpp
    src/game/SessionHud.cpp
    src/render/EntityRenderer.cpp
    src/net/AsioNetworkClient.cpp
    src/ui/Starfield.cpp
    src/ui/Button.cpp
    src/ui/TextInput.cpp
    src/ui/OptionSelector.cpp
    src/ui/GameConfig.cpp
    src/ui/MainMenu.cpp
)
target_include_directories(r-type_client PRIVATE src)
target_link_libraries(r-type_client PRIVATE rtype_server_lib rtype_common rtype_engine asio nlohmann_json::nlohmann_json SFML::Graphics SFML::Audio -Wl,--start-group freetype harfbuzz -Wl,--end-group)
rtype_set_warnings(r-type_client)
