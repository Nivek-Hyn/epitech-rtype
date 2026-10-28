cmake_minimum_required(VERSION 3.24)

project(RTypeEngine VERSION 0.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Warnings.cmake)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Threads REQUIRED)

# Generic game engine: knows nothing about R-Type. Never include anything from common/, server/ or client/ here.
add_library(rtype_engine STATIC
    src/Registry.cpp
    src/FixedTimestep.cpp
)
add_library(RTypeEngine::Engine ALIAS rtype_engine)
target_include_directories(rtype_engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(rtype_engine PUBLIC cxx_std_20)
target_link_libraries(rtype_engine PUBLIC Threads::Threads)
set_target_properties(rtype_engine PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME Engine
)
rtype_set_warnings(rtype_engine)

set(RTYPE_ENGINE_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/RTypeEngine)

install(TARGETS rtype_engine EXPORT RTypeEngineTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT RTypeEngineTargets
    FILE RTypeEngineTargets.cmake
    NAMESPACE RTypeEngine::
    DESTINATION ${RTYPE_ENGINE_CMAKE_INSTALL_DIR}
)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/RTypeEngineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/RTypeEngineConfig.cmake
    INSTALL_DESTINATION ${RTYPE_ENGINE_CMAKE_INSTALL_DIR}
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/RTypeEngineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/RTypeEngineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/RTypeEngineConfigVersion.cmake
    DESTINATION ${RTYPE_ENGINE_CMAKE_INSTALL_DIR}
)
