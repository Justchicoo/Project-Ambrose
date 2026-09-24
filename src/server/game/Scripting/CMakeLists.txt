# Project Ambrose by Imjustchico
# Builds the scripting library: the hook classes a content script derives from and the manager that calls them.
ambrose_add_library(scripting "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(scripting
    PUBLIC
        common)
