# Project Ambrose by Imjustchico
# Builds questtool, which scaffolds key-only quest updates and indexes quest references in local client data.
ambrose_add_executable(questtool "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(questtool PRIVATE shared)
ambrose_install_executable(questtool)
