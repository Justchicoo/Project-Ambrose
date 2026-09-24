# Project Ambrose by Imjustchico
# Builds dbimport, the tool that creates and updates every database without starting a server.
ambrose_add_executable(dbimport "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(dbimport PRIVATE database)
ambrose_copy_conf_dist(dbimport)
ambrose_copy_mariadb_plugins(dbimport)
