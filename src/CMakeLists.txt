# Project Ambrose by Imjustchico
# Adds the source layers in dependency order.
add_subdirectory(genrev)
add_subdirectory(common)
add_subdirectory(server)

if(TOOLS)
    add_subdirectory(tools)
endif()

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
