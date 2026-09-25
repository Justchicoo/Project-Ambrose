# Project Ambrose by Imjustchico
# Builds the server library: the game's own session, which the world owns and drains on its thread, the judge that spends a handoff key before that session is let in, and the way a let-in wizard is loaded, placed in its zone and handed its own object.
ambrose_add_library(gameserver-sessions "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(gameserver-sessions
    PUBLIC
        shared
        database
        accounts
        characters
        entities
        zones)
