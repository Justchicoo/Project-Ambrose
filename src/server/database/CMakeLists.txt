# Project Ambrose by Imjustchico
# Builds the database library: MariaDB Connector/C connections, query results and typed fields, pools, and the updater.
ambrose_add_library(database "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(database
    PUBLIC
        shared
    PRIVATE
        unofficial::libmariadb)
