# Project Ambrose by Imjustchico
# Builds the entities library: what an object in the world is, starting with the ids it carries, its runtime GID and its permID, and what the world database says about the client's object classes that the type dump does not.
ambrose_add_library(entities "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(entities
    PUBLIC
        database
        globals)
