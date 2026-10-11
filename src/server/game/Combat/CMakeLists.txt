# Project Ambrose by Imjustchico
# Builds the combat library: every sigil the user's install holds, read from the templates its manifest lists under Sigils/ into typed sigil records whose battlefield effects are read as spell effects are, and the duels a sigil's circles seat their participants on, with the objects the client draws them from.
ambrose_add_library(combat "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(combat
    PUBLIC
        spells)
