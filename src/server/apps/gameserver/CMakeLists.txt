# Project Ambrose by Imjustchico
# Builds the gameserver executable that runs one realm.
ambrose_add_executable(gameserver "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(gameserver PRIVATE database)
ambrose_copy_conf_dist(gameserver)
ambrose_copy_mariadb_plugins(gameserver)
