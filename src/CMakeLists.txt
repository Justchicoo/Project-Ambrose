# Project Ambrose by Imjustchico
# Adds the source layers in dependency order.
add_subdirectory(genrev)
add_subdirectory(common)

if(APPS_BUILD)
    add_subdirectory(server)
endif()
