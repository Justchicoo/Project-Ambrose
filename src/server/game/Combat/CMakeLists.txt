# Project Ambrose by Imjustchico
# Builds the combat library, starting with every sigil the user's install holds, read from the templates its manifest lists under Sigils/ into typed sigil records whose battlefield effects are read as spell effects are, and the creature decks the world database and a deck file on this machine hold, whose cards are the install's spells.
ambrose_add_library(combat "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(combat
    PUBLIC
        database
        spells
        unofficial::sqlite3::sqlite3)
