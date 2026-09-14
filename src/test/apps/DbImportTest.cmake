# Project Ambrose by Imjustchico
# With AMBROSE_TEST_DB set, runs dbimport to create and update three uniquely named databases twice, then once with a broken update that must exit 1 and name the file, and drops every database it made.
if(NOT APP OR NOT WORKDIR OR NOT SOURCE)
    message(FATAL_ERROR "APP, WORKDIR and SOURCE must be set")
endif()
if(NOT DEFINED ENV{AMBROSE_TEST_DB} OR "$ENV{AMBROSE_TEST_DB}" STREQUAL "")
    message("dbimport test skipped: AMBROSE_TEST_DB is not set")
    return()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/TestDatabases.cmake")

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")
get_filename_component(appDir "${APP}" DIRECTORY)
set(quietOptions --set "LogsDir=${WORKDIR}/logs" --set Appender.Console=1,3,0 --set Appender.DBImport=1,3,0)
ambrose_test_database_info(ambrose_dbimport_login loginInfo)
ambrose_test_database_info(ambrose_dbimport_characters characterInfo)
ambrose_test_database_info(ambrose_dbimport_world worldInfo)

foreach(round IN ITEMS first second)
    execute_process(COMMAND "${APP}" --config "${appDir}/dbimport.conf.dist" ${quietOptions}
            "--set=LoginDatabaseInfo=${loginInfo}" "--set=CharacterDatabaseInfo=${characterInfo}" "--set=WorldDatabaseInfo=${worldInfo}"
        WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE importResult OUTPUT_VARIABLE importOutput ERROR_VARIABLE importError TIMEOUT 120)
    if(NOT importResult EQUAL 0 OR NOT importOutput MATCHES "Every enabled database is created and up to date")
        ambrose_test_fail("dbimport's ${round} run exited ${importResult}: ${importOutput}${importError}")
    endif()
    if(round STREQUAL "first" AND NOT importOutput MATCHES "Created database ambrose_dbimport_world_")
        ambrose_test_fail("dbimport's first run did not create the world database: ${importOutput}")
    endif()
endforeach()
foreach(database IN ITEMS login characters world)
    if(NOT importOutput MATCHES "The ${database} database is up to date")
        ambrose_test_fail("dbimport's second run did not report the ${database} database up to date: ${importOutput}")
    endif()
endforeach()

file(COPY "${SOURCE}/data" DESTINATION "${WORKDIR}/source")
file(WRITE "${WORKDIR}/source/data/sql/updates/db_world/2099_01_01_00.sql" "-- Project Ambrose by Imjustchico\n-- A deliberately broken update for the dbimport test.\nSELEC broken;\n")
ambrose_test_database_info(ambrose_dbimport_broken_world brokenInfo)
execute_process(COMMAND "${APP}" --config "${appDir}/dbimport.conf.dist" ${quietOptions} --set Updates.EnableDatabases=4 "--set=Updates.SourcePath=${WORKDIR}/source" --set LoginDatabaseInfo= --set CharacterDatabaseInfo=
        "--set=WorldDatabaseInfo=${brokenInfo}"
    WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE brokenResult OUTPUT_VARIABLE brokenOutput ERROR_VARIABLE brokenError TIMEOUT 120)
if(NOT brokenResult EQUAL 1)
    ambrose_test_fail("dbimport with a broken update exited ${brokenResult}: ${brokenOutput}${brokenError}")
endif()
if(NOT brokenOutput MATCHES "2099_01_01_00\\.sql")
    ambrose_test_fail("dbimport with a broken update did not name the file: ${brokenOutput}${brokenError}")
endif()
ambrose_drop_test_databases()
