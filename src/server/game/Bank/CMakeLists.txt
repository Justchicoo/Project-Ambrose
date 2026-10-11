# Project Ambrose by Imjustchico
# Adds the bank's storage and its managers to the session library that owns the handlers and the world-thread lifecycle.
ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" bank_sources)
target_sources(gameserver-sessions PRIVATE ${bank_sources})
target_include_directories(gameserver-sessions PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
