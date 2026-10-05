# Project Ambrose by Imjustchico
# Builds quest data models, madlib blocks and client-facing quest and dialog ObjectProperty blob encoders.
ambrose_add_library(quests "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(quests
    PUBLIC
        entities)
