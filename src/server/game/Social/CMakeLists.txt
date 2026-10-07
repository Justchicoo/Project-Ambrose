# Project Ambrose by Imjustchico
# Adds social persistence and presence to the session library that owns the handlers and the world-thread lifecycle.
ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" social_sources)
target_sources(gameserver-sessions PRIVATE ${social_sources})
target_include_directories(gameserver-sessions PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
