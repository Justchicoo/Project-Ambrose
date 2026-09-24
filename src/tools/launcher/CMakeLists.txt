# Project Ambrose by Imjustchico
# Builds launcher, the program that starts the user's own Wizard101 client against an Ambrose login server: the launcher-core library holds the discovery, the run folder and the argument list that its tests drive, and the executable is a front end over it.
find_package(pugixml CONFIG REQUIRED)

ambrose_add_library(launcher-core "${CMAKE_CURRENT_SOURCE_DIR}/Core")
target_link_libraries(launcher-core
    PUBLIC
        shared
    PRIVATE
        pugixml::pugixml)

add_executable(launcher "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(launcher PRIVATE ambrose-compile-options launcher-core)
ambrose_copy_conf_dist(launcher)
