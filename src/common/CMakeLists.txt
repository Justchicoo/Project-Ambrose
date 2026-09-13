# Project Ambrose by Imjustchico
# Builds the common library of game-agnostic foundations.
ambrose_add_library(common "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(common
    PUBLIC
        fmt::fmt
    PRIVATE
        genrev)
