# Game logic as a library so it can be unit tested without the executable.
add_library(rtype_server_lib STATIC
    src/Game.cpp
    src/GameCollisions.cpp
    src/GameConfigLoader.cpp
    src/GameDifficulty.cpp
    src/GameLevels.cpp
    src/GamePlayers.cpp
    src/GameWaves.cpp
    src/net/AsioNetworkServer.cpp
    src/levelLoader.cpp
    src/MovementSystem.cpp
    src/EnemyShootSystem.cpp
)
target_include_directories(rtype_server_lib PUBLIC src)
target_link_libraries(rtype_server_lib PUBLIC rtype_common rtype_engine asio nlohmann_json::nlohmann_json)
rtype_set_warnings(rtype_server_lib)

add_executable(r-type_server src/main.cpp)
target_link_libraries(r-type_server PRIVATE rtype_server_lib)
rtype_set_warnings(r-type_server)
