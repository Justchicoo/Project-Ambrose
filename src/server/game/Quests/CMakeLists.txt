# Project Ambrose by Imjustchico
# Builds quest models, their validated database store and client blob encoders, and exports the actor-dialog model.
ambrose_add_library(quests "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(quests PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../Dialogs")

target_link_libraries(quests
    PUBLIC
        database
        entities)
