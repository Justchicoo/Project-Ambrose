# Project Ambrose by Imjustchico
# Builds the chat library: the command tree every CommandScript fills, who is running a command, and the lookup that judges one against the level its caller holds; the text of a chat line as the client packs it, the install's quick chat phrases, the custom-emote animation catalog and its ownership checks, and what a wizard says and who hears it.
ambrose_add_library(chat "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(chat
    PUBLIC
        scripting
        accounts
        entities)
