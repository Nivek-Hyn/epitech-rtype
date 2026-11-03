add_executable(rtype_tests
    test_aabb.cpp
    test_components.cpp
    test_event_bus.cpp
    test_fixed_timestep.cpp
    test_movement.cpp
    test_network_integration.cpp
    test_registry.cpp
    test_protocol.cpp
    test_game.cpp
    test_thread_safe_queue.cpp
    test_network_integration.cpp
)
target_link_libraries(rtype_tests PRIVATE rtype_server_lib GTest::gtest_main)
set_target_properties(rtype_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(GoogleTest)
gtest_discover_tests(rtype_tests WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
