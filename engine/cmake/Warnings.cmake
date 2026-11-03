include_guard(GLOBAL)

# rtype_set_warnings(<target>): common warning flags for our own targets (never for dependencies).
function(rtype_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    endif()
endfunction()
