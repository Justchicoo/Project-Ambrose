# Project Ambrose by Imjustchico
# Builds the chat library: the command tree and caller lookup, packed chat text, quick-chat phrases, UTF-16 chat-filter lists, and the speech a wizard says and who hears it.
ambrose_add_library(chat "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(chat
    PUBLIC
        scripting
        accounts)
