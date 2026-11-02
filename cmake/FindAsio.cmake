include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

CPMAddPackage(
    NAME asio
    GITHUB_REPOSITORY chriskohlhoff/asio
    GIT_TAG asio-1-38-2
    DOWNLOAD_ONLY YES
)
if(NOT TARGET asio)
    add_library(asio INTERFACE)
    target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/include)
    target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
    find_package(Threads REQUIRED)
    target_link_libraries(asio INTERFACE Threads::Threads)
    if(WIN32)
        target_compile_definitions(asio INTERFACE _WIN32_WINNT=0x0A00)
        target_link_libraries(asio INTERFACE ws2_32 wsock32)
    endif()
endif()

find_path(Asio_INCLUDE_DIR asio.hpp PATHS "${asio_SOURCE_DIR}/include" NO_DEFAULT_PATH NO_CACHE)
set(Asio_VERSION 1.38.2)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Asio REQUIRED_VARS Asio_INCLUDE_DIR asio_SOURCE_DIR
    VERSION_VAR Asio_VERSION)
