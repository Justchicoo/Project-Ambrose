# Project Ambrose by Imjustchico
# Builds the loginserver executable that authenticates accounts and lists characters.
ambrose_add_executable(loginserver "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(loginserver PRIVATE shared)
ambrose_copy_conf_dist(loginserver)
