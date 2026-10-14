# Project Ambrose by Imjustchico
# Builds the loot library: the world's loot tables, their entries and each creature template's tables, read into one reloadable generation and rolled for a wizard through the requirement evaluator and the live drop rates.
ambrose_add_library(loot "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(loot
    PUBLIC
        database
        conditions)
