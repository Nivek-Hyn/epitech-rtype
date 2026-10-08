# Generic game engine: knows nothing about R-Type. Never include anything from common/, server/ or client/ here.
add_library(rtype_engine STATIC
    src/Registry.cpp
    src/FixedTimestep.cpp
)
target_include_directories(rtype_engine PUBLIC include)
rtype_set_warnings(rtype_engine)
