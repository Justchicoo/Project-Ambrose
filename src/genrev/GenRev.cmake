# Project Ambrose by Imjustchico
# Reads the git hash, branch, and commit date and rewrites RevisionData.h only when they change.
set(rev_hash "unknown")
set(rev_branch "unknown")
set(rev_date "unknown")

find_program(AMBROSE_GIT git)

function(ambrose_git_query out)
    execute_process(
        COMMAND "${AMBROSE_GIT}" ${ARGN}
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE value
        RESULT_VARIABLE result
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(result EQUAL 0 AND NOT value STREQUAL "")
        string(REPLACE "\\" "" value "${value}")
        string(REPLACE "\"" "" value "${value}")
        set(${out} "${value}" PARENT_SCOPE)
    endif()
endfunction()

if(AMBROSE_GIT AND EXISTS "${SOURCE_DIR}/.git")
    ambrose_git_query(rev_hash rev-parse --short HEAD)
    ambrose_git_query(rev_branch rev-parse --abbrev-ref HEAD)
    ambrose_git_query(rev_date log -1 --format=%cs)
endif()

configure_file("${TEMPLATE}" "${OUTPUT}" @ONLY)
