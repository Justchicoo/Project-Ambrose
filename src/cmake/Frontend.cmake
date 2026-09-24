# Project Ambrose by Imjustchico
# Builds the panel and the launcher window's page when FRONTEND is on, and says plainly what it left out when it is off, so a machine with no Node still builds the servers.
function(ambrose_frontend)
    if(NOT FRONTEND)
        message(STATUS "FRONTEND is off: the panel (apps/dashboard) and the launcher window's page (apps/launcherui) are not built.")
        message(STATUS "FRONTEND is off: the servers build and run without them, and the supervisor serves a placeholder page.")
        message(STATUS "FRONTEND is off: build them with -DFRONTEND=ON on a machine with Node 24 and npm 11.")
        return()
    endif()

    find_program(AMBROSE_NPM NAMES npm npm.cmd)
    if(NOT AMBROSE_NPM)
        message(FATAL_ERROR "FRONTEND is on but npm was not found. Install Node 24 with npm 11, or configure with -DFRONTEND=OFF.")
    endif()

    set(install_flags ci --ignore-scripts)
    if(EXISTS "${CMAKE_SOURCE_DIR}/.npm-cache")
        list(APPEND install_flags --offline --cache "${CMAKE_SOURCE_DIR}/.npm-cache")
    endif()

    add_custom_target(frontend ALL
        COMMAND "${AMBROSE_NPM}" ${install_flags}
        COMMAND "${AMBROSE_NPM}" run build
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building the panel and the launcher window's page"
        VERBATIM)
endfunction()
