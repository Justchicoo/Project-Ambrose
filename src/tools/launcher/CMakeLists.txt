# Project Ambrose by Imjustchico
# Builds launcher, the program that starts the user's own Wizard101 client against an Ambrose login server: the launcher-core library holds the discovery, the run folder, the argument list and the channel the window speaks over, all of which its tests drive, the executable is a front end over it, the window is WebView2 on Windows and WebKitGTK wherever the build finds it, with a build that finds neither keeping the console launcher, and the built window page is put beside the program so a window opened from a build tree reads the same page an installed one does.
find_package(pugixml CONFIG REQUIRED)

ambrose_add_library(launcher-core "${CMAKE_CURRENT_SOURCE_DIR}/Core")
target_link_libraries(launcher-core
    PUBLIC
        shared
    PRIVATE
        pugixml::pugixml
        nlohmann_json::nlohmann_json)

if(WIN32)
    find_package(unofficial-webview2 CONFIG REQUIRED)
    target_link_libraries(launcher-core PRIVATE unofficial::webview2::webview2)
else()
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(webkitgtk IMPORTED_TARGET webkit2gtk-4.1 gtk+-3.0)
    endif()
    if(webkitgtk_FOUND)
        target_link_libraries(launcher-core PRIVATE PkgConfig::webkitgtk)
        target_compile_definitions(launcher-core PRIVATE AMBROSE_HAS_WEBKITGTK)
    endif()
endif()

add_executable(launcher "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(launcher PRIVATE ambrose-compile-options launcher-core)
ambrose_copy_conf_dist(launcher)

set(launcher_page "${PROJECT_SOURCE_DIR}/apps/launcherui/dist")
if(EXISTS "${launcher_page}/index.html")
    add_custom_target(launcher-page
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "$<TARGET_FILE_DIR:launcher>/launcher-ui"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${launcher_page}" "$<TARGET_FILE_DIR:launcher>/launcher-ui"
        VERBATIM)
    add_dependencies(launcher launcher-page)
    install(DIRECTORY "${launcher_page}/" DESTINATION bin/launcher-ui)
endif()
