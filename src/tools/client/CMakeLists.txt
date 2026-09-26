# Project Ambrose by Imjustchico
# Builds client, the one tool that asks the user's own install a question: the classes its type dump holds, what its messages carry and where the client program registers their handlers, what the program's code and virtual tables do, the entries of its archives and the game object blobs its messages carry, reading what the world database adds to the dump when it is given one.
ambrose_add_executable(client "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(client PRIVATE entities client-image)
ambrose_install_executable(client)
ambrose_copy_mariadb_plugins(client)
