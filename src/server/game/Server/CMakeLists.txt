# Project Ambrose by Imjustchico
# Builds the server library: the game's own session, which the world owns and drains on its thread.
ambrose_add_library(gameserver-sessions "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(gameserver-sessions
    PUBLIC
        shared)
