# Project Ambrose by Imjustchico
# Builds the quests library: the authored quest tables of the world database read into a checked generation of quests with their goals, dialogs and lookups, served by sQuestMgr and reloaded as one set through quest_template.
ambrose_add_library(quests "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(quests
    PUBLIC
        database)
