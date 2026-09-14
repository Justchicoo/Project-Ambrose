# Project Ambrose by Imjustchico
# Builds the database library: MariaDB Connector/C connections, query results and typed fields, pools, the updater, and the DB log appender.
ambrose_add_library(database "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(database
    PUBLIC
        shared
    PRIVATE
        unofficial::libmariadb)

target_compile_definitions(database PRIVATE "AMBROSE_SOURCE_DIRECTORY=\"${PROJECT_SOURCE_DIR}\"")
