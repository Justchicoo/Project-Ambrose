# Project Ambrose by Imjustchico
# Adds the game session's message handlers, one file per subsystem, to the session library that declares them.
ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" handler_sources)
target_sources(gameserver-sessions PRIVATE ${handler_sources})
