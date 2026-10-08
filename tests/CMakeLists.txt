add_executable(rtype_tests
    test_registry.cpp
    test_protocol.cpp
    test_game.cpp
)
target_link_libraries(rtype_tests PRIVATE rtype_server_lib GTest::gtest_main)
set_target_properties(rtype_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(GoogleTest)
gtest_discover_tests(rtype_tests)
