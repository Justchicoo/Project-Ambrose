# Project Ambrose by Imjustchico
# Builds the combat library, starting with every sigil the user's install holds, read from the templates its manifest lists under Sigils/ into typed sigil records whose battlefield effects are read as spell effects are.
ambrose_add_library(combat "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(combat
    PUBLIC
        spells)
