# Project Ambrose by Imjustchico
# Builds the accounts library: account management, account settings and the verifier key ring over the login database.
ambrose_add_library(accounts "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(accounts
    PUBLIC
        database)
