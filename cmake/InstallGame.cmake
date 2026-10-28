# Keep executables and assets together: the current client resolves assets from its working directory.
# Use --component GameRuntime to avoid installing dependency headers and development libraries.
install(TARGETS r-type_server RUNTIME DESTINATION . COMPONENT GameRuntime)

if(RTYPE_BUILD_CLIENT)
    install(TARGETS r-type_client RUNTIME DESTINATION . COMPONENT GameRuntime)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets" DESTINATION . COMPONENT GameRuntime)
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/InstalledReadme.txt.in"
    "${PROJECT_BINARY_DIR}/RUNNING.txt"
    @ONLY
)
install(FILES "${PROJECT_BINARY_DIR}/RUNNING.txt" DESTINATION . COMPONENT GameRuntime)
