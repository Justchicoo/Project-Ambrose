# Project Ambrose by Imjustchico
# Builds launcher, the program that starts the user's own Wizard101 client against an Ambrose login server: the launcher-core library holds the discovery, the run folder, the argument list and the channel the window speaks over, all of which its tests drive, the executable is a front end over it, the window is the desktop shell's, and the built window page is compiled into launcher-core, so a window opened from a build tree or an install reads the page from memory, and a build without the front end compiles in a page that says so.
find_package(pugixml CONFIG REQUIRED)

ambrose_add_library(launcher-core "${CMAKE_CURRENT_SOURCE_DIR}/Core")
target_link_libraries(launcher-core
    PUBLIC
        shared
        desktop-shell
    PRIVATE
        pugixml::pugixml
        nlohmann_json::nlohmann_json)

set(launcher_page_depends "")
if(TARGET frontend)
    set(launcher_page_depends frontend)
endif()
ambrose_embed_page(launcher-core NAME LauncherPage FOLDER "${PROJECT_SOURCE_DIR}/apps/launcherui/dist"
    PLACEHOLDER "This launcher was built without its window page. Build it with -DFRONTEND=ON, or run the launcher from a terminal."
    DEPENDS ${launcher_page_depends})

add_executable(launcher "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(launcher PRIVATE ambrose-compile-options launcher-core)
ambrose_copy_conf_dist(launcher)
