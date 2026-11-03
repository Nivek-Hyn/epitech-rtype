include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.17.0
    OPTIONS
        "INSTALL_GTEST OFF"
        "gtest_force_shared_crt ON"
)

set(GTest_TestTarget FALSE)
if(TARGET GTest::gtest_main)
    set(GTest_TestTarget TRUE)
endif()
set(GTest_VERSION 1.17.0)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GTest REQUIRED_VARS googletest_SOURCE_DIR GTest_TestTarget
    VERSION_VAR GTest_VERSION)
