cmake_minimum_required(VERSION 3.24)

# Run from any directory with: cmake -P engine/cmake/CheckPackage.cmake
get_filename_component(engine_source "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED ENGINE_CHECK_ROOT)
    set(ENGINE_CHECK_ROOT "${engine_source}/../build/engine-package-check-portable")
endif()
get_filename_component(ENGINE_CHECK_ROOT "${ENGINE_CHECK_ROOT}" ABSOLUTE)
if(NOT DEFINED ENGINE_CHECK_CONFIGURATION)
    set(ENGINE_CHECK_CONFIGURATION Debug)
endif()

set(generator_arguments)
if(DEFINED ENGINE_CHECK_GENERATOR)
    list(APPEND generator_arguments -G "${ENGINE_CHECK_GENERATOR}")
endif()

function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE command_result)
    if(NOT "${command_result}" STREQUAL "0")
        message(FATAL_ERROR "Package check failed (${command_result}): ${ARGV}")
    endif()
endfunction()

# A space in the install path also checks that consumers receive quoted paths.
set(package_prefix "${ENGINE_CHECK_ROOT}/installed engine")
run_checked("${CMAKE_COMMAND}" --fresh -S "${engine_source}" -B "${ENGINE_CHECK_ROOT}/engine"
    ${generator_arguments}
    "-DCMAKE_BUILD_TYPE=${ENGINE_CHECK_CONFIGURATION}"
    -DRTYPE_ENGINE_BUILD_ECS=ON -DRTYPE_ENGINE_BUILD_CORE=ON
    -DRTYPE_ENGINE_BUILD_PHYSICS=ON -DRTYPE_ENGINE_BUILD_NET=ON
    -DRTYPE_ENGINE_BUILD_SYSTEMS=ON)
run_checked("${CMAKE_COMMAND}" --build "${ENGINE_CHECK_ROOT}/engine"
    --config "${ENGINE_CHECK_CONFIGURATION}" --parallel 4)
run_checked("${CMAKE_COMMAND}" --install "${ENGINE_CHECK_ROOT}/engine"
    --config "${ENGINE_CHECK_CONFIGURATION}" --prefix "${package_prefix}")

# This separate project links the installed package, without add_subdirectory(engine).
run_checked("${CMAKE_COMMAND}" --fresh -S "${engine_source}/examples/basic"
    -B "${ENGINE_CHECK_ROOT}/consumer" ${generator_arguments}
    "-DCMAKE_BUILD_TYPE=${ENGINE_CHECK_CONFIGURATION}"
    "-DCMAKE_PREFIX_PATH=${package_prefix}" -DRTypeEngine_DIR=RTypeEngine_DIR-NOTFOUND)
run_checked("${CMAKE_COMMAND}" --build "${ENGINE_CHECK_ROOT}/consumer"
    --config "${ENGINE_CHECK_CONFIGURATION}" --parallel 4)
get_filename_component(cmake_directory "${CMAKE_COMMAND}" DIRECTORY)
find_program(ctest_program NAMES ctest HINTS "${cmake_directory}" REQUIRED)
run_checked("${ctest_program}" --test-dir "${ENGINE_CHECK_ROOT}/consumer"
    -C "${ENGINE_CHECK_CONFIGURATION}" --output-on-failure --no-tests=error)

message(STATUS "Engine package check passed (${ENGINE_CHECK_CONFIGURATION}).")
