# Project Ambrose by Imjustchico
# Builds the globals library: process-wide services every game system shares, starting with object id generation.
ambrose_add_library(globals "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(globals
    PUBLIC
        common)
