# Project Ambrose by Imjustchico
# Builds the results library: the runner that decodes the result lists zone triggers and quests hold and runs each result, in order, through the handler its owning system registers for its class.
ambrose_add_library(results "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(results
    PUBLIC
        shared)
