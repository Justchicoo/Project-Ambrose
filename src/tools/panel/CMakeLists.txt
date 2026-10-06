# Project Ambrose by Imjustchico
# Builds panel, the program that lists panels and opens each in a window of its own with its screens from apps/panelui compiled in, over panel-core, its own work over the desktop shell: its list of panels in a SQLite file of its own, the addresses and certificate modes it trusts, the pairing lines it reads and the probe that names whether each panel answers, sent through libcurl with a pinned certificate checked inside the handshake, all of which its tests drive with no window.
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

ambrose_add_library(panel-core "${CMAKE_CURRENT_SOURCE_DIR}/Core")
target_link_libraries(panel-core
    PUBLIC
        shared
        desktop-shell
    PRIVATE
        unofficial::sqlite3::sqlite3
        CURL::libcurl
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json)

set(panel_page_depends "")
if(TARGET frontend)
    set(panel_page_depends frontend)
endif()
add_executable(panel "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(panel PRIVATE ambrose-compile-options panel-core nlohmann_json::nlohmann_json)
ambrose_embed_page(panel NAME PanelUiPage FOLDER "${PROJECT_SOURCE_DIR}/apps/panelui/dist"
    PLACEHOLDER "This panel program was built without its screens. Build it with -DFRONTEND=ON."
    DEPENDS ${panel_page_depends})
ambrose_install_executable(panel)
ambrose_copy_conf_dist(panel)
