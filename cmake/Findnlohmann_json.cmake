include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    GIT_TAG v3.11.3
    OPTIONS "JSON_BuildTests OFF"
)

set(nlohmann_json_TargetAvailable FALSE)
if(TARGET nlohmann_json::nlohmann_json)
    set(nlohmann_json_TargetAvailable TRUE)
endif()
set(nlohmann_json_VERSION 3.11.3)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(nlohmann_json
    REQUIRED_VARS nlohmann_json_SOURCE_DIR nlohmann_json_TargetAvailable
    VERSION_VAR nlohmann_json_VERSION)
