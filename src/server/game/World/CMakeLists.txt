# Project Ambrose by Imjustchico
# Builds the world library: the update loop, the sessions it owns and the tick that carries every script's OnUpdate.
ambrose_add_library(world "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(world
    PUBLIC
        gameserver-sessions
        movement
        scripting)
