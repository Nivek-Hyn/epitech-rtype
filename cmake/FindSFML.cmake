include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

# FreeType includes FindPkgConfig while this find module is active. Its package
# name is intentionally different from SFML; limit this allowance to that build.
set(_rtype_previous_name_mismatched "${FPHSA_NAME_MISMATCHED}")
set(FPHSA_NAME_MISMATCHED TRUE)
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
set(FPHSA_NAME_MISMATCHED "${_rtype_previous_name_mismatched}")
unset(_rtype_previous_name_mismatched)

set(SFML_RuntimeTargets FALSE)
if(TARGET SFML::Graphics AND TARGET SFML::Audio)
    set(SFML_RuntimeTargets TRUE)
endif()
set(SFML_VERSION 3.1.0)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SFML REQUIRED_VARS SFML_SOURCE_DIR SFML_RuntimeTargets
    VERSION_VAR SFML_VERSION)
