# Project Ambrose by Imjustchico
# Builds the supervision library of app definitions, the per-app controller, output capture, the saved state, the panel's own SQLite store with its users and their Argon2id passwords and the supervisor's admin routes that tests link, and the supervisor executable that starts, stops, restarts and takes back the other apps on top of it.
if(TARGET botan::botan)
    set(ambrose_botan_target botan::botan)
else()
    set(ambrose_botan_target botan::botan-static)
endif()

ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" supervision_sources)
list(FILTER supervision_sources EXCLUDE REGEX "/Main\\.cpp$")
add_library(supervision STATIC ${supervision_sources})
ambrose_collect_include_dirs("${CMAKE_CURRENT_SOURCE_DIR}" supervision_include_dirs)
target_include_directories(supervision PUBLIC ${supervision_include_dirs})
target_link_libraries(supervision PUBLIC ambrose-compile-options database PRIVATE unofficial::sqlite3::sqlite3 ${ambrose_botan_target})

add_executable(supervisor "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(supervisor PRIVATE ambrose-compile-options supervision)
ambrose_copy_conf_dist(supervisor)
