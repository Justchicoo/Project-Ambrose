# Project Ambrose by Imjustchico
# Regenerates RevisionData.h on every build and exposes it through the genrev target.
set(AMBROSE_GENREV_DIR "${CMAKE_BINARY_DIR}/src/genrev")
set(AMBROSE_GENREV_ARGS
    -D "SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    -D "TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/RevisionData.h.in"
    -D "OUTPUT=${AMBROSE_GENREV_DIR}/RevisionData.h")

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${AMBROSE_GENREV_ARGS} -P "${CMAKE_CURRENT_SOURCE_DIR}/GenRev.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_custom_target(revision_data ALL
    COMMAND "${CMAKE_COMMAND}" ${AMBROSE_GENREV_ARGS} -P "${CMAKE_CURRENT_SOURCE_DIR}/GenRev.cmake"
    BYPRODUCTS "${AMBROSE_GENREV_DIR}/RevisionData.h"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Checking git revision")

add_library(genrev INTERFACE)
target_include_directories(genrev INTERFACE "${AMBROSE_GENREV_DIR}")
add_dependencies(genrev revision_data)
