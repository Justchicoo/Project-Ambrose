# Project Ambrose by Imjustchico
# Builds the login library of sessions, message rules and console commands that tests link, and the loginserver executable that authenticates accounts and lists characters on top of it.
ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" login_sources)
list(FILTER login_sources EXCLUDE REGEX "/Main\\.cpp$")
add_library(login STATIC ${login_sources})
ambrose_collect_include_dirs("${CMAKE_CURRENT_SOURCE_DIR}" login_include_dirs)
target_include_directories(login PUBLIC ${login_include_dirs})
target_link_libraries(login PUBLIC ambrose-compile-options accounts)

add_executable(loginserver "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(loginserver PRIVATE ambrose-compile-options login)
ambrose_copy_conf_dist(loginserver)
ambrose_copy_mariadb_plugins(loginserver)
