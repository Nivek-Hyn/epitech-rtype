add_executable(r-type_client
    src/main.cpp
    src/ResourceManager.cpp
    src/net/AsioNetworkClient.cpp
    src/ui/Starfield.cpp
    src/ui/Button.cpp
    src/ui/TextInput.cpp
    src/ui/OptionSelector.cpp
    src/ui/MainMenu.cpp
)
target_include_directories(r-type_client PRIVATE src)
target_link_libraries(r-type_client PRIVATE rtype_common rtype_engine asio SFML::Graphics SFML::Audio -Wl,--start-group freetype harfbuzz -Wl,--end-group)
rtype_set_warnings(r-type_client)
