# Project Ambrose by Imjustchico
# Runs the studio program without a window to check its usage text, that bad usage exits 2, that with no install and no world database --answer still prints the summary with nothing loaded and refuses a message that is not an object, and, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name the user's own install and type dump, that the install's cinematics are listed and one is decoded into its stages and actors; it reports itself skipped when the client checks cannot run.
if(NOT APP OR NOT WORKDIR)
    message(FATAL_ERROR "APP and WORKDIR must be set")
endif()
file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}/no-install")

execute_process(COMMAND "${APP}" --help RESULT_VARIABLE helpResult OUTPUT_VARIABLE helpOutput ERROR_VARIABLE helpError TIMEOUT 30)
if(NOT helpResult EQUAL 0 OR NOT helpOutput MATCHES "Usage: studio" OR NOT helpOutput MATCHES "--answer <json>")
    message(FATAL_ERROR "studio --help exited ${helpResult}: ${helpOutput}${helpError}")
endif()

foreach(arguments IN ITEMS "--bogus" "--answer" "extra" "--client;--answer;{}")
    execute_process(COMMAND "${APP}" ${arguments} RESULT_VARIABLE usageResult OUTPUT_VARIABLE usageOutput ERROR_VARIABLE usageError TIMEOUT 30)
    if(NOT usageResult EQUAL 2 OR NOT usageError MATCHES "studio: .*Usage: studio")
        message(FATAL_ERROR "studio with arguments '${arguments}' exited ${usageResult} instead of 2: ${usageOutput}${usageError}")
    endif()
endforeach()

set(ENV{AMBROSE_SETUP_MODE} "off")
set(ENV{AMBROSE_WORLD_DATABASE_INFO} "")
execute_process(COMMAND "${APP}" --client "${WORKDIR}/no-install" --answer "{\"id\":1,\"path\":\"/studio/summary\"}"
    RESULT_VARIABLE emptyResult OUTPUT_VARIABLE emptyOutput ERROR_VARIABLE emptyError TIMEOUT 60)
if(NOT emptyResult EQUAL 0 OR NOT emptyOutput MATCHES "\"quests\":0" OR NOT emptyOutput MATCHES "\"install\":false" OR NOT emptyError MATCHES "no world database is named")
    message(FATAL_ERROR "studio with no install exited ${emptyResult}: ${emptyOutput}${emptyError}")
endif()

execute_process(COMMAND "${APP}" --client "${WORKDIR}/no-install" --answer "[1]" RESULT_VARIABLE arrayResult OUTPUT_VARIABLE arrayOutput ERROR_VARIABLE arrayError TIMEOUT 60)
if(NOT arrayResult EQUAL 2 OR NOT arrayError MATCHES "is not a JSON object")
    message(FATAL_ERROR "studio answering an array exited ${arrayResult}: ${arrayOutput}${arrayError}")
endif()

if("$ENV{AMBROSE_CLIENT_DIR}" STREQUAL "" OR "$ENV{AMBROSE_TYPE_DUMP_PATH}" STREQUAL "")
    message(STATUS "studio test skipped: its client checks need AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH")
    return()
endif()

execute_process(COMMAND "${APP}" --client "$ENV{AMBROSE_CLIENT_DIR}" --type-dump "$ENV{AMBROSE_TYPE_DUMP_PATH}"
    --answer "{\"id\":2,\"path\":\"/studio/cinematics\",\"body\":{\"search\":\"Mellori\"}}"
    --answer "{\"id\":3,\"path\":\"/studio/cinematic\",\"body\":{\"name\":\"EM-MelloriRavenCinematic\"}}"
    RESULT_VARIABLE installResult OUTPUT_VARIABLE installOutput ERROR_VARIABLE installError TIMEOUT 300)
if(NOT installResult EQUAL 0 OR NOT installOutput MATCHES "\"id\":2,\"status\":200" OR NOT installOutput MATCHES "Cinematics/EM-MelloriRavenCinematic.xml"
    OR NOT installOutput MATCHES "\"id\":3,\"status\":200" OR NOT installOutput MATCHES "\"m_stages\"" OR NOT installOutput MATCHES "\"m_actors\"")
    message(FATAL_ERROR "studio against the install exited ${installResult}: ${installOutput}${installError}")
endif()
