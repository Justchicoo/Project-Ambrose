# Project Ambrose by Imjustchico
# Builds the zones library: the zone templates, the named places inside them and the objects placed in them, held in memory and rebuilt without a restart, and the running instances of those zones with the mobile ids each hands out.
ambrose_add_library(zones "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(zones
    PUBLIC
        database
        entities
    PRIVATE
        nlohmann_json::nlohmann_json)
