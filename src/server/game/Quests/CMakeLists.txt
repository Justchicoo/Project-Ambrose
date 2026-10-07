# Project Ambrose by Imjustchico
# Builds quest data models, madlib blocks and client-facing quest and dialog ObjectProperty blob encoders, and exports the Dialogs folder's actor dialog model.
ambrose_add_library(quests "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(quests PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../Dialogs")

target_link_libraries(quests
    PUBLIC
        entities)
