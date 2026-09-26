# Project Ambrose by Imjustchico
# Builds the spells library: every spell the user's install holds, read from the templates its manifest lists under Spells/ into typed spell records, found by template id, which is the hash of the spell's name, and reloaded as one checked set of name-keyed templates.
ambrose_add_library(spells "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(spells
    PUBLIC
        entities)
