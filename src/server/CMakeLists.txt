# Project Ambrose by Imjustchico
# Adds the server layers in dependency order, then the server apps.
add_subdirectory(shared)
add_subdirectory(database)
add_subdirectory(game)
add_subdirectory(scripts)

if(APPS_BUILD)
    add_subdirectory(apps)
endif()
