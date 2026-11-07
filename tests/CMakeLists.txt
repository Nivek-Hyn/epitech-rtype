add_executable(rtype_tests
    test_aabb.cpp
    test_components.cpp
    test_event_bus.cpp
    test_fixed_timestep.cpp
    test_movement.cpp
    test_network_integration.cpp
    test_registry.cpp
    test_prefabs.cpp
    test_protocol.cpp
    test_game.cpp
    test_weapons.cpp
    test_level_editor.cpp
    test_brickrush.cpp
    test_thread_safe_queue.cpp
    test_network_client.cpp
    test_traffic_stats.cpp
    test_sequence_window.cpp
    test_reliable_queue.cpp
    test_snapshot_interpolator.cpp
    ${PROJECT_SOURCE_DIR}/client/src/net/AsioNetworkClient.cpp
)
# AsioNetworkClient.cpp (client network layer, no SFML) is tested here and includes "net/AsioNetworkClient.hpp".
target_include_directories(rtype_tests PRIVATE ${PROJECT_SOURCE_DIR}/client/src)
target_link_libraries(rtype_tests PRIVATE rtype_server_lib level_editor_core brickrush_core GTest::gtest_main nlohmann_json::nlohmann_json)

# The client settings need SFML, which a server-only build does not download.
if(RTYPE_BUILD_CLIENT)
    target_sources(rtype_tests PRIVATE test_game_config.cpp ../client/src/ui/GameConfig.cpp)
    target_link_libraries(rtype_tests PRIVATE SFML::Window)
endif()
set_target_properties(rtype_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(GoogleTest)
gtest_discover_tests(rtype_tests WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
