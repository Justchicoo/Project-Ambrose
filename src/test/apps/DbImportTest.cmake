# Project Ambrose by Imjustchico
# With AMBROSE_TEST_DB set, runs dbimport to create and update three databases twice, then once with a broken update that must exit 1 and name the file.
if(NOT APP OR NOT WORKDIR OR NOT SOURCE)
    message(FATAL_ERROR "APP, WORKDIR and SOURCE must be set")
endif()
if(NOT DEFINED ENV{AMBROSE_TEST_DB} OR "$ENV{AMBROSE_TEST_DB}" STREQUAL "")
    message("dbimport test skipped: AMBROSE_TEST_DB is not set")
    return()
endif()

function(database_info name out)
    set(parts "$ENV{AMBROSE_TEST_DB}")
    list(REMOVE_AT parts 4)
    list(INSERT parts 4 "${name}")
    list(JOIN parts ";" joined)
    set(${out} "${joined}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")
get_filename_component(appDir "${APP}" DIRECTORY)
set(quietOptions --set "LogsDir=${WORKDIR}/logs" --set Appender.Console=1,3,0 --set Appender.DBImport=1,3,0)
database_info(ambrose_ci_login loginInfo)
database_info(ambrose_ci_characters characterInfo)
database_info(ambrose_ci_world worldInfo)

foreach(round IN ITEMS first second)
    execute_process(COMMAND "${APP}" --config "${appDir}/dbimport.conf.dist" ${quietOptions}
            "--set=LoginDatabaseInfo=${loginInfo}" "--set=CharacterDatabaseInfo=${characterInfo}" "--set=WorldDatabaseInfo=${worldInfo}"
        WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE importResult OUTPUT_VARIABLE importOutput ERROR_VARIABLE importError TIMEOUT 120)
    if(NOT importResult EQUAL 0 OR NOT importOutput MATCHES "Every enabled database is created and up to date")
        message(FATAL_ERROR "dbimport's ${round} run exited ${importResult}: ${importOutput}${importError}")
    endif()
endforeach()
foreach(database IN ITEMS login characters world)
    if(NOT importOutput MATCHES "The ${database} database is up to date")
        message(FATAL_ERROR "dbimport's second run did not report the ${database} database up to date: ${importOutput}")
    endif()
endforeach()

file(COPY "${SOURCE}/data" DESTINATION "${WORKDIR}/source")
file(WRITE "${WORKDIR}/source/data/sql/updates/db_world/2099_01_01_00.sql" "-- Project Ambrose by Imjustchico\n-- A deliberately broken update for the dbimport test.\nSELEC broken;\n")
database_info(ambrose_ci_broken_world brokenInfo)
execute_process(COMMAND "${APP}" --config "${appDir}/dbimport.conf.dist" ${quietOptions} --set Updates.EnableDatabases=4 "--set=Updates.SourcePath=${WORKDIR}/source" --set LoginDatabaseInfo= --set CharacterDatabaseInfo=
        "--set=WorldDatabaseInfo=${brokenInfo}"
    WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE brokenResult OUTPUT_VARIABLE brokenOutput ERROR_VARIABLE brokenError TIMEOUT 120)
if(NOT brokenResult EQUAL 1)
    message(FATAL_ERROR "dbimport with a broken update exited ${brokenResult}: ${brokenOutput}${brokenError}")
endif()
if(NOT brokenOutput MATCHES "2099_01_01_00\\.sql")
    message(FATAL_ERROR "dbimport with a broken update did not name the file: ${brokenOutput}${brokenError}")
endif()
