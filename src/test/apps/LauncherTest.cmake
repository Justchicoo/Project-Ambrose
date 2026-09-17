# Project Ambrose by Imjustchico
# Runs the built launcher without ever starting a client: it checks the usage text, that bad usage exits 2 and a named settings file that is missing exits 1, that a folder holding no install, patching asked for through the environment and a port of 0 each exit 1 naming the cause, that on a machine holding a synthetic install AMBROSE_SETUP_MODE=off prints the find and the flag to pass while auto uses it and names the missing client program, that an install whose archive holds no defaultconfig.xml is named, that --dry-run then prints the run folder and the whole command and writes nothing, that a run folder inside the install and a value beginning with '-' are refused, and, when AMBROSE_CLIENT_DIR names the user's own install, that --dry-run against it prints -L, -P 0, -A, -D and -G with none of the launcher's own environment variables set; it reports itself skipped when that last check cannot run.
if(NOT APP OR NOT WORKDIR)
    message(FATAL_ERROR "APP and WORKDIR must be set")
endif()
file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")
set(settings "${WORKDIR}/launcher.conf")
file(WRITE "${settings}" "# Project Ambrose by Imjustchico\n# Settings for a launcher test.\n")

execute_process(COMMAND "${APP}" --help RESULT_VARIABLE helpResult OUTPUT_VARIABLE helpOutput ERROR_VARIABLE helpError TIMEOUT 30)
if(NOT helpResult EQUAL 0 OR NOT helpOutput MATCHES "Usage: launcher" OR NOT helpOutput MATCHES "--dry-run" OR NOT helpOutput MATCHES "--user <id> <key> \\[name\\]")
    message(FATAL_ERROR "launcher --help exited ${helpResult}: ${helpOutput}${helpError}")
endif()

foreach(arguments IN ITEMS "--bogus" "extra" "--client" "--host;--port;1" "--user;1" "--window" "--character")
    execute_process(COMMAND "${APP}" ${arguments} RESULT_VARIABLE usageResult OUTPUT_VARIABLE usageOutput ERROR_VARIABLE usageError TIMEOUT 30)
    if(NOT usageResult EQUAL 2 OR NOT usageError MATCHES "launcher: .*Usage: launcher")
        message(FATAL_ERROR "launcher with arguments '${arguments}' exited ${usageResult} instead of 2: ${usageOutput}${usageError}")
    endif()
endforeach()

execute_process(COMMAND "${APP}" --config "${WORKDIR}/missing.conf" --dry-run RESULT_VARIABLE missingResult OUTPUT_VARIABLE missingOutput ERROR_VARIABLE missingError TIMEOUT 30)
if(NOT missingResult EQUAL 1 OR NOT missingError MATCHES "missing\\.conf.*configuration file not found")
    message(FATAL_ERROR "launcher with a missing settings file exited ${missingResult}: ${missingOutput}${missingError}")
endif()

file(MAKE_DIRECTORY "${WORKDIR}/no-install")
execute_process(COMMAND "${APP}" --config "${settings}" --dry-run --client "${WORKDIR}/no-install" RESULT_VARIABLE emptyResult OUTPUT_VARIABLE emptyOutput ERROR_VARIABLE emptyError TIMEOUT 30)
if(NOT emptyResult EQUAL 1 OR NOT emptyError MATCHES "holds no Wizard101 install")
    message(FATAL_ERROR "launcher with a folder that holds no install exited ${emptyResult}: ${emptyOutput}${emptyError}")
endif()

set(machine "${WORKDIR}/machine")
set(synthetic "${machine}/drive_c/ProgramData/KingsIsle Entertainment/Wizard101")
file(WRITE "${synthetic}/Data/GameData/Root.wad" "not an archive")
file(WRITE "${synthetic}/Bin/revision.dat" "r999999999.Synthetic_1_0\n")
set(settingsEnv --unset=AMBROSE_TYPE_DUMP_PATH --unset=AMBROSE_SETUP_MODE --unset=AMBROSE_LOGIN_HOST --unset=AMBROSE_LOGIN_PORT
    --unset=AMBROSE_LOCALE --unset=AMBROSE_WINDOW --unset=AMBROSE_WINDOW_X --unset=AMBROSE_WINDOW_Y --unset=AMBROSE_FULLSCREEN --unset=AMBROSE_RUN_DIR --unset=AMBROSE_PATCH)
set(machineEnv "ProgramData=${machine}/drive_c/ProgramData" "WINEPREFIX=${machine}" "LOCALAPPDATA=${WORKDIR}/data" "XDG_DATA_HOME=${WORKDIR}/data"
    --unset=AMBROSE_CLIENT_DIR ${settingsEnv})

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${machineEnv} AMBROSE_SETUP_MODE=off "${APP}" --config "${settings}" --dry-run RESULT_VARIABLE offResult OUTPUT_VARIABLE offOutput ERROR_VARIABLE offError TIMEOUT 60)
if(NOT offResult EQUAL 1 OR NOT offError MATCHES "launcher: Wizard101 was found on this machine: [^\n]*r999999999\\.Synthetic_1_0" OR NOT offError MATCHES "Pass --client with one of them")
    message(FATAL_ERROR "launcher with AMBROSE_SETUP_MODE=off did not print the install it found and the flag to pass (${offResult}): ${offOutput}${offError}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${machineEnv} "${APP}" --config "${settings}" --dry-run RESULT_VARIABLE noProgramResult OUTPUT_VARIABLE noProgramOutput ERROR_VARIABLE noProgramError TIMEOUT 60)
if(NOT noProgramResult EQUAL 1 OR NOT noProgramError MATCHES "WizardGraphicalClient\\.exe is missing")
    message(FATAL_ERROR "launcher with an install that has no client program exited ${noProgramResult}: ${noProgramOutput}${noProgramError}")
endif()

file(WRITE "${synthetic}/Bin/WizardGraphicalClient.exe" "not a program")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${machineEnv} AMBROSE_PATCH=1 "${APP}" --config "${settings}" --dry-run RESULT_VARIABLE patchResult OUTPUT_VARIABLE patchOutput ERROR_VARIABLE patchError TIMEOUT 60)
if(NOT patchResult EQUAL 1 OR NOT patchError MATCHES "patching is asked for")
    message(FATAL_ERROR "launcher with patching asked for exited ${patchResult}: ${patchOutput}${patchError}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${machineEnv} "${APP}" --config "${settings}" --dry-run --port 0 RESULT_VARIABLE portResult OUTPUT_VARIABLE portOutput ERROR_VARIABLE portError TIMEOUT 60)
if(NOT portResult EQUAL 1 OR NOT portError MATCHES "login port 0 is not a number from 1 to 65535")
    message(FATAL_ERROR "launcher with a port of 0 exited ${portResult}: ${portOutput}${portError}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${machineEnv} "${APP}" --config "${settings}" --dry-run RESULT_VARIABLE noConfigResult OUTPUT_VARIABLE noConfigOutput ERROR_VARIABLE noConfigError TIMEOUT 60)
if(NOT noConfigResult EQUAL 1 OR NOT noConfigError MATCHES "defaultconfig\\.xml could not be read from its Root\\.wad")
    message(FATAL_ERROR "launcher with an install that has no client configuration exited ${noConfigResult}: ${noConfigOutput}${noConfigError}")
endif()

file(WRITE "${synthetic}/Bin/config.xml" "<?xml version=\"1.0\" ?>\n<config>\n</config>\n")
set(run "${WORKDIR}/run")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${machineEnv} "${APP}" --config "${settings}" --dry-run --run-dir "${run}" --host 127.0.0.1 --port 12100 --locale de-DE --window 800x600
    RESULT_VARIABLE dryResult OUTPUT_VARIABLE dryOutput ERROR_VARIABLE dryError TIMEOUT 60)
if(NOT dryResult EQUAL 0
    OR NOT dryOutput MATCHES "launcher: install [^\n]*r999999999\\.Synthetic_1_0"
    OR NOT dryOutput MATCHES "launcher: run folder [^\n]*run"
    OR NOT dryOutput MATCHES "WizardGraphicalClient.exe[^\n]* -L 127\\.0\\.0\\.1 12100 -P 0 -A de-DE -D [^\n]*GameData[^\n]* -G [^\n]*WizardClient\\.log"
    OR NOT dryOutput MATCHES "nothing was started")
    message(FATAL_ERROR "launcher --dry-run on a synthetic install exited ${dryResult}: ${dryOutput}${dryError}")
endif()
if(EXISTS "${run}")
    message(FATAL_ERROR "launcher --dry-run wrote ${run}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${machineEnv} "${APP}" --config "${settings}" --dry-run --run-dir "${synthetic}/Bin"
    RESULT_VARIABLE insideResult OUTPUT_VARIABLE insideOutput ERROR_VARIABLE insideError TIMEOUT 60)
if(NOT insideResult EQUAL 1 OR NOT insideError MATCHES "cannot be inside the install")
    message(FATAL_ERROR "launcher with a run folder inside the install exited ${insideResult}: ${insideOutput}${insideError}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${machineEnv} "${APP}" --config "${settings}" --dry-run --run-dir "${run}" --character -ST
    RESULT_VARIABLE dashResult OUTPUT_VARIABLE dashOutput ERROR_VARIABLE dashError TIMEOUT 60)
if(NOT dashResult EQUAL 1 OR NOT dashError MATCHES "character name .* begins with '-'")
    message(FATAL_ERROR "launcher with a character name beginning with '-' exited ${dashResult}: ${dashOutput}${dashError}")
endif()

if("$ENV{AMBROSE_CLIENT_DIR}" STREQUAL "")
    message(STATUS "launcher test skipped: its client check needs AMBROSE_CLIENT_DIR")
    return()
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${settingsEnv} "${APP}" --config "${settings}" --dry-run --run-dir "${WORKDIR}/own"
    RESULT_VARIABLE ownResult OUTPUT_VARIABLE ownOutput ERROR_VARIABLE ownError TIMEOUT 120)
if(NOT ownResult EQUAL 0
    OR NOT ownOutput MATCHES "WizardGraphicalClient.exe[^\n]* -L 127\\.0\\.0\\.1 12000 -P 0 -A en-US -D [^\n]*GameData[^\n]* -G [^\n]*WizardClient\\.log"
    OR NOT ownOutput MATCHES "launcher: run folder [^\n]*own")
    message(FATAL_ERROR "launcher --dry-run against the user's own install exited ${ownResult}: ${ownOutput}${ownError}")
endif()
if(EXISTS "${WORKDIR}/own")
    message(FATAL_ERROR "launcher --dry-run wrote ${WORKDIR}/own")
endif()
