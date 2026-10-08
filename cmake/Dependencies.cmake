# All third-party dependencies are fetched here with CPM.
# Set CPM_SOURCE_CACHE (e.g. ~/.cache/CPM) to avoid re-downloading them for every build directory.

include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

# Asio (standalone, header-only): networking
CPMAddPackage(
    NAME asio
    GITHUB_REPOSITORY chriskohlhoff/asio
    GIT_TAG asio-1-38-2
    DOWNLOAD_ONLY YES
)
if(asio_ADDED)
    add_library(asio INTERFACE)
    target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
    target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
    find_package(Threads REQUIRED)
    target_link_libraries(asio INTERFACE Threads::Threads)
    if(WIN32)
        target_compile_definitions(asio INTERFACE _WIN32_WINNT=0x0A00)
        target_link_libraries(asio INTERFACE ws2_32 wsock32)
    endif()
endif()

# SFML 3: rendering, audio, input (client only)
if(RTYPE_BUILD_CLIENT)
    CPMAddPackage(
        NAME SFML
        GITHUB_REPOSITORY SFML/SFML
        GIT_TAG 3.1.0
        OPTIONS
            "SFML_BUILD_NETWORK OFF"
            "SFML_BUILD_EXAMPLES OFF"
            "SFML_BUILD_DOC OFF"
            # Fetch FreeType, HarfBuzz, Vorbis, FLAC... instead of requiring them on the system (default on Linux)
            "SFML_USE_SYSTEM_DEPS OFF"
            "BUILD_SHARED_LIBS OFF"
    )
endif()

# GoogleTest: unit tests
if(RTYPE_BUILD_TESTS)
    CPMAddPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        GIT_TAG v1.17.0
        OPTIONS
            "INSTALL_GTEST OFF"
            "gtest_force_shared_crt ON"
    )
endif()
