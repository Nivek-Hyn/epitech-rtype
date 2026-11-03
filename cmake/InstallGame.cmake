# Keep executables and assets together: the current client resolves assets from its working directory.
# Use --component GameRuntime to avoid installing dependency headers and development libraries.
install(TARGETS r-type_server RUNTIME DESTINATION . COMPONENT GameRuntime)

if(RTYPE_BUILD_CLIENT)
    install(TARGETS r-type_client RUNTIME DESTINATION . COMPONENT GameRuntime)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets" DESTINATION . COMPONENT GameRuntime)
else()
    # The server loads its level configuration even when graphics are disabled.
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets/levels" DESTINATION assets COMPONENT GameRuntime)
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/InstalledReadme.txt.in"
    "${PROJECT_BINARY_DIR}/RUNNING.txt"
    @ONLY
)
install(FILES "${PROJECT_BINARY_DIR}/RUNNING.txt" DESTINATION . COMPONENT GameRuntime)

include("${CMAKE_CURRENT_LIST_DIR}/InstallNotices.cmake")

if(MSVC)
    # Ask CMake for the toolchain's redistributable runtime; do not copy Windows system DLLs.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    set(CMAKE_INSTALL_DEBUG_LIBRARIES FALSE)
    set(CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY FALSE)
    include(InstallRequiredSystemLibraries)
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION . COMPONENT GameRuntime
            CONFIGURATIONS Release RelWithDebInfo MinSizeRel
        )
    endif()
endif()
