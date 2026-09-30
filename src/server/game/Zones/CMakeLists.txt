# Project Ambrose by Imjustchico
# Builds the zones library: the zone templates, the named places inside them and the objects placed in them, held in memory and rebuilt without a restart, the running instances of those zones with the mobile ids each hands out, and the cell grid and per-wizard visibility sets that decide what each wizard is shown.
ambrose_add_library(zones "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(zones
    PUBLIC
        database
        entities
    PRIVATE
        nlohmann_json::nlohmann_json)
