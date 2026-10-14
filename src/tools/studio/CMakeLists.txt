# Project Ambrose by Imjustchico
# Builds studio, the content editor: the studio-core library holds what the Studio reads from the world database's quest tables and the user's own install and the channel its window speaks over, both of which its tests drive, the executable is a front end over it, the window is the desktop shell's, and the built page is compiled into studio-core, so a build without the front end compiles in a page that says so.
ambrose_add_library(studio-core "${CMAKE_CURRENT_SOURCE_DIR}/Core")
target_link_libraries(studio-core
    PUBLIC
        quests
        desktop-shell
        nlohmann_json::nlohmann_json)

set(studio_page_depends "")
if(TARGET frontend)
    set(studio_page_depends frontend)
endif()
ambrose_embed_page(studio-core NAME StudioPage FOLDER "${PROJECT_SOURCE_DIR}/apps/studioui/dist"
    PLACEHOLDER "This Studio was built without its window page. Build it with -DFRONTEND=ON."
    DEPENDS ${studio_page_depends})

add_executable(studio "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(studio PRIVATE ambrose-compile-options studio-core)
ambrose_install_executable(studio)
ambrose_copy_mariadb_plugins(studio)
