# Project Ambrose by Imjustchico
# Builds the supervision library of app definitions, the per-app controller, output capture, the saved state, the panel's own SQLite store with its users and their Argon2id passwords, its single-use sign-in links, the panel user console commands and the command line that asks a running supervisor for a link, the panel's event socket with its catalog, streams and tickets, its keyring wrapped on Windows with the data protection API, the file roots with their routes, and the supervisor's admin routes that tests link, and the supervisor executable that starts, stops, restarts and takes back the other apps on top of it with the built dashboard compiled in, which both listeners serve while their DashboardDir options are empty. The supervision library links libcurl for the panel's test mail and captcha checks.
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
target_link_libraries(supervision PUBLIC ambrose-compile-options database PRIVATE unofficial::sqlite3::sqlite3 ${ambrose_botan_target} CURL::libcurl)
if(WIN32)
    target_link_libraries(supervision PRIVATE crypt32)
endif()

add_executable(supervisor "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(supervisor PRIVATE ambrose-compile-options supervision)
if(WIN32)
    target_link_libraries(supervisor PRIVATE advapi32 shell32)
endif()
ambrose_copy_conf_dist(supervisor)

set(dashboard_depends "")
if(TARGET frontend)
    set(dashboard_depends frontend)
endif()
ambrose_embed_page(supervisor NAME DashboardPage FOLDER "${PROJECT_SOURCE_DIR}/apps/dashboard/dist"
    PLACEHOLDER "This supervisor was built without the panel. Build it with -DFRONTEND=ON, or point Panel.DashboardDir at a built apps/dashboard/dist."
    DEPENDS ${dashboard_depends})
