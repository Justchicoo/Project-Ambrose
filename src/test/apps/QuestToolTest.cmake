# Project Ambrose by Imjustchico
# Checks questtool command help, reference argument validation, malformed scaffold arguments, and the key-only tables and Prep binding in an isolated work folder.
if(NOT APP OR NOT WORKDIR)
    message(FATAL_ERROR "APP and WORKDIR must be set")
endif()
file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")

execute_process(COMMAND "${APP}" --help RESULT_VARIABLE helpResult OUTPUT_VARIABLE helpOutput ERROR_VARIABLE helpError TIMEOUT 30)
if(NOT helpResult EQUAL 0 OR NOT helpOutput MATCHES "Usage: questtool <command>" OR NOT helpOutput MATCHES "--lang-stem" OR NOT helpOutput MATCHES "--classes")
    message(FATAL_ERROR "questtool --help exited ${helpResult}: ${helpOutput}${helpError}")
endif()

execute_process(COMMAND "${APP}" refs RESULT_VARIABLE refsResult OUTPUT_VARIABLE refsOutput ERROR_VARIABLE refsError TIMEOUT 30)
if(NOT refsResult EQUAL 2 OR NOT refsError MATCHES "refs requires --classes")
    message(FATAL_ERROR "questtool refs accepted missing class data (${refsResult}): ${refsOutput}${refsError}")
endif()

execute_process(COMMAND "${APP}" scaffold --title-key "Not a locale key" --giver 123 --lang-stem WizQst000001
    WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE invalidResult OUTPUT_VARIABLE invalidOutput ERROR_VARIABLE invalidError TIMEOUT 30)
if(NOT invalidResult EQUAL 2 OR NOT invalidError MATCHES "questtool: --title-key")
    message(FATAL_ERROR "questtool accepted a display title as a key (${invalidResult}): ${invalidOutput}${invalidError}")
endif()

execute_process(COMMAND "${APP}" scaffold --title-key QuestTitle_000001 --giver 123 --lang-stem WizQst000001
    WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE scaffoldResult OUTPUT_VARIABLE scaffoldOutput ERROR_VARIABLE scaffoldError TIMEOUT 30)
if(NOT scaffoldResult EQUAL 0)
    message(FATAL_ERROR "questtool scaffold exited ${scaffoldResult}: ${scaffoldOutput}${scaffoldError}")
endif()
file(GLOB updates "${WORKDIR}/data/sql/updates/pending_db_world/rev_*_quest_WizQst000001.sql")
list(LENGTH updates updateCount)
if(NOT updateCount EQUAL 1)
    message(FATAL_ERROR "questtool wrote ${updateCount} matching update files: ${scaffoldOutput}${scaffoldError}")
endif()
list(GET updates 0 update)
get_filename_component(name "${update}" NAME)
if(NOT name MATCHES "^rev_[0-9]+_quest_WizQst000001\\.sql$")
    message(FATAL_ERROR "questtool wrote an invalid pending update name: ${name}")
endif()
file(READ "${update}" sql)
foreach(expected IN ITEMS
        "INSERT INTO `quest_template`"
        "INSERT INTO `quest_start_goal`"
        "INSERT INTO `quest_goal`"
        "'persona'"
        "INSERT INTO `quest_dialog`"
        "INSERT INTO `quest_dialog_entry`"
        "'Prep', 0, 123")
    if(NOT sql MATCHES "${expected}")
        message(FATAL_ERROR "questtool scaffold omitted ${expected}: ${sql}")
    endif()
endforeach()
