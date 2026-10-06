# Project Ambrose by Imjustchico
# Builds the items library, starting with every item template the user's install holds, read from the templates its manifest lists under ObjectData/ into typed item records and reloaded as one checked set.
ambrose_add_library(items "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(items
    PUBLIC
        entities)
