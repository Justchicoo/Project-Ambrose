# Project Ambrose by Imjustchico
# Runs a built server executable to check --version, a missing config, and --check with its shipped .conf.dist; with AMBROSE_TEST_DB the game server and the login server, with its login and characters databases, create, update, open and close uniquely named databases that are dropped afterwards, the login server runs account commands piped into its console input, and a bad login database string exits 1.
if(NOT APP OR NOT NAME OR NOT WORKDIR)
    message(FATAL_ERROR "APP, NAME and WORKDIR must be set")
endif()

execute_process(COMMAND "${APP}" --version RESULT_VARIABLE versionResult OUTPUT_VARIABLE versionOutput ERROR_VARIABLE versionError TIMEOUT 30)
if(NOT versionResult EQUAL 0)
    message(FATAL_ERROR "${NAME} --version exited ${versionResult}: ${versionOutput}${versionError}")
endif()
if(NOT versionOutput MATCHES "Project Ambrose rev ")
    message(FATAL_ERROR "${NAME} --version printed '${versionOutput}'")
endif()

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")
execute_process(COMMAND "${APP}" WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE missingResult OUTPUT_VARIABLE missingOutput ERROR_VARIABLE missingError TIMEOUT 30)
if(NOT missingResult EQUAL 1)
    message(FATAL_ERROR "${NAME} without a config exited ${missingResult}: ${missingOutput}${missingError}")
endif()
if(NOT missingError MATCHES "app-smoke[/\\\\]${NAME}[/\\\\]${NAME}\\.conf")
    message(FATAL_ERROR "${NAME} without a config did not name its full path: ${missingError}")
endif()
if(NOT missingError MATCHES "${NAME}\\.conf\\.dist")
    message(FATAL_ERROR "${NAME} without a config did not suggest copying ${NAME}.conf.dist: ${missingError}")
endif()

get_filename_component(appDir "${APP}" DIRECTORY)
include("${CMAKE_CURRENT_LIST_DIR}/TestDatabases.cmake")
set(portOption "${NAME}")
string(REPLACE "gameserver" "WorldServerPort" portOption "${portOption}")
string(REPLACE "loginserver" "LoginServerPort" portOption "${portOption}")
string(REPLACE "patchserver" "PatchServerPort" portOption "${portOption}")
set(quietOptions --set BindIP=127.0.0.1 --set ${portOption}=0 --set ClientDir= --set Appender.Server=1,3,0 --set Appender.Errors=1,3,0 --set Appender.Stream=1,3,0 --set Appender.Console=1,3,0)
execute_process(COMMAND "${APP}" --check --config "${appDir}/${NAME}.conf.dist" ${quietOptions} --set LoginDatabaseInfo= --set CharacterDatabaseInfo= --set WorldDatabaseInfo=
    WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE distResult OUTPUT_VARIABLE distOutput ERROR_VARIABLE distError TIMEOUT 30)
if(NOT distResult EQUAL 0 OR NOT distOutput MATCHES "${NAME} ready" OR NOT distOutput MATCHES "${NAME} stopped")
    message(FATAL_ERROR "${NAME} with its shipped ${NAME}.conf.dist did not report ready (${distResult}): ${distOutput}${distError}")
endif()

if(NAME STREQUAL "loginserver" AND DEFINED ENV{AMBROSE_TEST_DB} AND NOT "$ENV{AMBROSE_TEST_DB}" STREQUAL "")
    ambrose_test_database_info(ambrose_smoke_login smokeDatabase)
    ambrose_test_database_info(ambrose_smoke_characters smokeCharacters)
    foreach(round IN ITEMS first second)
        execute_process(COMMAND "${APP}" --check --config "${appDir}/${NAME}.conf.dist" ${quietOptions} "--set=LoginDatabaseInfo=${smokeDatabase}" "--set=CharacterDatabaseInfo=${smokeCharacters}"
            WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE databaseResult OUTPUT_VARIABLE databaseOutput ERROR_VARIABLE databaseError TIMEOUT 60)
        if(NOT databaseResult EQUAL 0 OR NOT databaseOutput MATCHES "Opened database connection pool login: 1 async, 1 sync" OR NOT databaseOutput MATCHES "loginserver ready" OR NOT databaseOutput MATCHES "Closed database connection pool login"
            OR NOT databaseOutput MATCHES "Opened database connection pool characters: 1 async, 1 sync")
            ambrose_test_fail("loginserver with AMBROSE_TEST_DB did not update, open and close the login pool on its ${round} start (${databaseResult}): ${databaseOutput}${databaseError}")
        endif()
        if(round STREQUAL "first" AND (NOT databaseOutput MATCHES "Created database ambrose_smoke_login_" OR NOT databaseOutput MATCHES "importing 2 base file" OR NOT databaseOutput MATCHES "Applied RELEASED 2026_01_01_00\\.sql to the login database"
            OR NOT databaseOutput MATCHES "Created database ambrose_smoke_characters_" OR NOT databaseOutput MATCHES "Applied [0-9]+ update\\(s\\) to the characters database"))
            ambrose_test_fail("loginserver's first start did not create the login and characters databases, import the login base and apply their updates: ${databaseOutput}")
        endif()
    endforeach()
    if(NOT databaseOutput MATCHES "The login database is up to date" OR NOT databaseOutput MATCHES "The characters database is up to date")
        ambrose_test_fail("loginserver's second start did not report the login and characters databases up to date: ${databaseOutput}")
    endif()

    file(WRITE "${WORKDIR}/console.txt" "help account\naccount create smoke_user smoke_secret\naccount create SMOKE_USER smoke_secret\naccount info Smoke_User\nshutdown\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E cat "${WORKDIR}/console.txt"
        COMMAND "${APP}" --config "${appDir}/${NAME}.conf.dist" ${quietOptions} "--set=LoginDatabaseInfo=${smokeDatabase}" "--set=CharacterDatabaseInfo=${smokeCharacters}"
        WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE consoleResult OUTPUT_VARIABLE consoleOutput ERROR_VARIABLE consoleError TIMEOUT 60)
    if(NOT consoleResult EQUAL 0)
        ambrose_test_fail("loginserver reading console commands exited ${consoleResult}: ${consoleOutput}${consoleError}")
    endif()
    foreach(expected IN ITEMS "account create <username> <password> \\[email\\] - create an account" "Account smoke_user created with id [0-9]+" "Account SMOKE_USER already exists"
            "Account smoke_user \\(id [0-9]+\\): security level 0, not locked, verifier key 0" "Not banned" "loginserver is shutting down"
            "Console: account create \\(arguments hidden\\)" "loginserver shutting down after the shutdown command" "loginserver stopped")
        if(NOT consoleOutput MATCHES "${expected}")
            ambrose_test_fail("loginserver's console run did not print '${expected}': ${consoleOutput}${consoleError}")
        endif()
    endforeach()
    if(consoleOutput MATCHES "smoke_secret" OR consoleError MATCHES "smoke_secret")
        ambrose_test_fail("loginserver's console run printed a password: ${consoleOutput}${consoleError}")
    endif()
endif()

if(NAME STREQUAL "gameserver" AND DEFINED ENV{AMBROSE_TEST_DB} AND NOT "$ENV{AMBROSE_TEST_DB}" STREQUAL "")
    set(realmOptions)
    foreach(database IN ITEMS Login Character World)
        string(TOLOWER "${database}" lowerDatabase)
        ambrose_test_database_info(ambrose_smoke_${lowerDatabase} databaseInfo)
        string(REPLACE ";" "\\;" databaseInfo "${databaseInfo}")
        list(APPEND realmOptions "--set=${database}DatabaseInfo=${databaseInfo}")
    endforeach()
    execute_process(COMMAND "${APP}" --check --config "${appDir}/${NAME}.conf.dist" ${quietOptions} ${realmOptions} --set Appender.DB=4,2,0
        WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE realmResult OUTPUT_VARIABLE realmOutput ERROR_VARIABLE realmError TIMEOUT 120)
    if(NOT realmResult EQUAL 0)
        ambrose_test_fail("gameserver --check on empty databases exited ${realmResult}: ${realmOutput}${realmError}")
    endif()
    foreach(expected IN ITEMS "Created database ambrose_smoke_login_" "Created database ambrose_smoke_character_" "Created database ambrose_smoke_world_"
            "The login database is up to date|Applied [0-9]+ update\\(s\\) to the login database"
            "Applied [0-9]+ update\\(s\\) to the characters database" "Applied [0-9]+ update\\(s\\) to the world database"
            "Opened database connection pool world: 1 async, 1 sync" "gameserver ready" "Closed database connection pool world" "gameserver stopped")
        if(NOT realmOutput MATCHES "${expected}")
            ambrose_test_fail("gameserver on empty databases did not log '${expected}': ${realmOutput}${realmError}")
        endif()
    endforeach()
endif()
ambrose_drop_test_databases()

if(NAME STREQUAL "loginserver")
    execute_process(COMMAND "${APP}" --check --config "${appDir}/${NAME}.conf.dist" ${quietOptions} "--set=LoginDatabaseInfo=not a connection string"
        WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE badResult OUTPUT_VARIABLE badOutput ERROR_VARIABLE badError TIMEOUT 30)
    if(NOT badResult EQUAL 1)
        message(FATAL_ERROR "loginserver with a bad LoginDatabaseInfo exited ${badResult}: ${badOutput}${badError}")
    endif()
    if(NOT badOutput MATCHES "LoginDatabaseInfo is not a valid connection string")
        message(FATAL_ERROR "loginserver with a bad LoginDatabaseInfo did not explain why: ${badOutput}${badError}")
    endif()
endif()
