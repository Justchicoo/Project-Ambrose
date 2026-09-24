# Project Ambrose by Imjustchico
# Builds client, the one tool that asks the user's own install a question: the classes its type dump holds, what its messages carry, and the entries of its archives.
ambrose_add_executable(client "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(client PRIVATE shared)
ambrose_install_executable(client)
