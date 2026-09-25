# Project Ambrose by Imjustchico
# Builds the entities library: what an object in the world is, starting with the ids it carries, its runtime GID and its permID.
ambrose_add_library(entities "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(entities
    PUBLIC
        globals)
