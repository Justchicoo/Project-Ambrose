# Project Ambrose by Imjustchico
# Builds the extractor tool, a front end over the client data extraction in shared and the world SQL scripts in the database library.
ambrose_add_executable(extractor "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(extractor PRIVATE database)
ambrose_copy_mariadb_plugins(extractor)
install(TARGETS extractor RUNTIME DESTINATION bin)
