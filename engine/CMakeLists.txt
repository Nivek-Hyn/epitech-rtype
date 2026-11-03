cmake_minimum_required(VERSION 3.24)

project(RTypeEngine VERSION 0.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Warnings.cmake)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(RTYPE_ENGINE_BUILD_ECS "Build entity and component management" ON)
option(RTYPE_ENGINE_BUILD_CORE "Build timing, events and thread-safe queues" ON)
option(RTYPE_ENGINE_BUILD_PHYSICS "Provide AABB collision detection" ON)
option(RTYPE_ENGINE_BUILD_NET "Provide binary buffer utilities" ON)
option(RTYPE_ENGINE_BUILD_SYSTEMS "Provide generic ECS systems" ON)

if(RTYPE_ENGINE_BUILD_SYSTEMS AND NOT RTYPE_ENGINE_BUILD_ECS)
    message(FATAL_ERROR "Systems requires ECS. Enable RTYPE_ENGINE_BUILD_ECS or disable RTYPE_ENGINE_BUILD_SYSTEMS.")
endif()

# Keep the existing target as an umbrella for all enabled modules.
add_library(rtype_engine INTERFACE)
add_library(RTypeEngine::Engine ALIAS rtype_engine)
set_target_properties(rtype_engine PROPERTIES EXPORT_NAME Engine)
set(engineTargets rtype_engine)

foreach(module IN ITEMS ECS Core Physics Net Systems)
    string(TOUPPER ${module} optionSuffix)
    if(NOT RTYPE_ENGINE_BUILD_${optionSuffix})
        continue()
    endif()

    string(TOLOWER ${module} moduleDirectory)
    set(moduleTarget rtype_engine_${moduleDirectory})
    if(module STREQUAL "ECS")
        add_library(${moduleTarget} STATIC src/Registry.cpp)
        set(moduleScope PUBLIC)
    elseif(module STREQUAL "Core")
        add_library(${moduleTarget} STATIC src/FixedTimestep.cpp)
        set(moduleScope PUBLIC)
        find_package(Threads REQUIRED)
        target_link_libraries(${moduleTarget} PUBLIC Threads::Threads)
    else()
        add_library(${moduleTarget} INTERFACE)
        set(moduleScope INTERFACE)
    endif()

    add_library(RTypeEngine::${module} ALIAS ${moduleTarget})
    target_include_directories(${moduleTarget} ${moduleScope}
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
    target_compile_features(${moduleTarget} ${moduleScope} cxx_std_20)
    set_target_properties(${moduleTarget} PROPERTIES EXPORT_NAME ${module})
    if(moduleScope STREQUAL "PUBLIC")
        set_target_properties(${moduleTarget} PROPERTIES CXX_EXTENSIONS OFF)
        rtype_set_warnings(${moduleTarget})
    endif()

    target_link_libraries(rtype_engine INTERFACE ${moduleTarget})
    list(APPEND engineTargets ${moduleTarget})
    install(DIRECTORY include/engine/${moduleDirectory}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/engine/${moduleDirectory}
    )
endforeach()

if(RTYPE_ENGINE_BUILD_SYSTEMS)
    target_link_libraries(rtype_engine_systems INTERFACE rtype_engine_ecs)
endif()

set(RTYPE_ENGINE_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/RTypeEngine)

install(TARGETS ${engineTargets} EXPORT RTypeEngineTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
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
