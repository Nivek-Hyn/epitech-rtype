# R-Type code shared by the server and the client: protocol, shared components, constants.
add_library(rtype_common STATIC
    src/Protocol.cpp
)
target_include_directories(rtype_common PUBLIC include)
target_link_libraries(rtype_common PRIVATE rtype_engine)
rtype_set_warnings(rtype_common)
