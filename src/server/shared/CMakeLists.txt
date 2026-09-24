# Project Ambrose by Imjustchico
# Builds the shared library of code every server uses: network, messages, ObjectProperty, archives, locale text, character name tables, client data discovery and extraction, realms, the folder the project's shipped SQL is found in, and the admin API listener Crow serves, with the certificate OpenSSL serves it over TLS with.
ambrose_add_library(shared "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(shared
    PUBLIC
        common
    PRIVATE
        Crow::Crow
        pugixml::pugixml
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto)

target_compile_definitions(shared PRIVATE CROW_ENABLE_LOGGING CROW_DISABLE_STATIC_DIR CROW_ENABLE_SSL "AMBROSE_SOURCE_DIRECTORY=\"${PROJECT_SOURCE_DIR}\"")

if(WIN32)
    target_link_libraries(shared PRIVATE advapi32)
endif()
