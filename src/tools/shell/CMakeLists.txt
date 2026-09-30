# Project Ambrose by Imjustchico
# Builds desktop-shell, the window host the launcher and the panel program share: WebView2 on Windows and WebKitGTK wherever the build finds it, with a build that finds neither still linking and saying it has no window, plus the rules, profiles and remembered place its tests drive without one.
ambrose_add_library(desktop-shell "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(desktop-shell
    PUBLIC
        shared
    PRIVATE
        nlohmann_json::nlohmann_json)

if(WIN32)
    find_package(unofficial-webview2 CONFIG REQUIRED)
    target_link_libraries(desktop-shell PRIVATE unofficial::webview2::webview2 shlwapi comdlg32 shell32)
else()
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(webkitgtk IMPORTED_TARGET webkit2gtk-4.1 gtk+-3.0)
    endif()
    if(webkitgtk_FOUND)
        target_link_libraries(desktop-shell PRIVATE PkgConfig::webkitgtk)
        target_compile_definitions(desktop-shell PUBLIC AMBROSE_HAS_WEBKITGTK)
    endif()
endif()
