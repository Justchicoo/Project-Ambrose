# Project Ambrose by Imjustchico
# Builds the chat library: command dispatch, packed chat text, quick-chat phrases, UTF-16 filters, custom-emote ownership, and queued speech.
ambrose_add_library(chat "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(chat
    PUBLIC
        scripting
        accounts
        entities)
