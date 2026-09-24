# Project Ambrose by Imjustchico
# Builds the gameserver executable that runs one realm, with every script CMake found linked into it.
ambrose_add_executable(gameserver "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(gameserver PRIVATE characters scripts)
ambrose_copy_conf_dist(gameserver)
ambrose_copy_mariadb_plugins(gameserver)
