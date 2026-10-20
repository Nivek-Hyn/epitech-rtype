add_executable(r-type_client
    src/main.cpp
    src/ResourceManager.cpp
    src/net/AsioNetworkClient.cpp
)
target_include_directories(r-type_client PRIVATE src)
target_link_libraries(r-type_client PRIVATE rtype_common rtype_engine asio SFML::Graphics SFML::Audio)
rtype_set_warnings(r-type_client)

# Assets are loaded from the working directory: run the client from the repository root.
