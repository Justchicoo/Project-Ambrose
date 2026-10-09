# Project Ambrose by Imjustchico
# Builds the requirement evaluator, which loads world-authored requirement lists into one reloadable generation and exposes script-defined condition hooks.

ambrose_add_library(conditions "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(conditions
    PUBLIC
        database
        scripting
)
