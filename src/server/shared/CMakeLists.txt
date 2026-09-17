# Project Ambrose by Imjustchico
# Builds the shared library of code every server uses: network, messages, ObjectProperty, archives, locale text, character name tables, client data discovery and extraction, and realms.
ambrose_add_library(shared "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(shared
    PUBLIC
        common
    PRIVATE
        pugixml::pugixml
        nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(shared PRIVATE advapi32)
endif()
