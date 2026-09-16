# Project Ambrose by Imjustchico
# Builds the extraction library, which reads the user's own install into world database rows and writes them as SQL, and the extractor tool over it.
ambrose_add_library(extraction "${CMAKE_CURRENT_SOURCE_DIR}/Core")

target_link_libraries(extraction
    PUBLIC
        database
    PRIVATE
        pugixml::pugixml)

add_executable(extractor "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")
target_link_libraries(extractor PRIVATE ambrose-compile-options extraction)
ambrose_copy_mariadb_plugins(extractor)
install(TARGETS extractor RUNTIME DESTINATION bin)
