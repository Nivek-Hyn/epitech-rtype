# Each direct dependency has its own find module backed by CPM.
# Set CPM_SOURCE_CACHE (e.g. ~/.cache/CPM) to share downloads between build directories.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

find_package(Asio REQUIRED MODULE)
if(RTYPE_BUILD_CLIENT)
    find_package(SFML REQUIRED MODULE)
endif()
if(RTYPE_BUILD_TESTS)
    find_package(GTest REQUIRED MODULE)
endif()

# nlohmann_json: JSON parsing
CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    GIT_TAG v3.11.3
    OPTIONS
        "JSON_BuildTests OFF"
)
