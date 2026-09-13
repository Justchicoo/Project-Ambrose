# Project Ambrose by Imjustchico
# Adds the source layers in dependency order.
add_subdirectory(genrev)
add_subdirectory(common)
add_subdirectory(server)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
