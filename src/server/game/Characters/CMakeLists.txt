# Project Ambrose by Imjustchico
# Builds the characters library: stored wizards, their appearance and soft deletion over the characters database.
ambrose_add_library(characters "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(characters
    PUBLIC
        database)
