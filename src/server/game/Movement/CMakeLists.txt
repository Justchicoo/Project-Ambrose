# Project Ambrose by Imjustchico
# Builds the world movement math used by gameserver coordinate and direction messages.
ambrose_add_library(movement "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(movement PUBLIC "${PROJECT_SOURCE_DIR}/src/common")
target_link_libraries(movement PUBLIC common)
