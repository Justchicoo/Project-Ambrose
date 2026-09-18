# Project Ambrose by Imjustchico
# Registers the client driver's self-tests, which need no client, and one client-labelled test per scenario, which skips with 77 on a machine that cannot drive a client; every test carries the label driver, and only the scenarios carry client, because a CTest label filter is a regular expression and a label holding the word client would take the self-tests with it.
find_package(Python3 COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
    add_test(NAME clientdriver.selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_clientdriver.py")
    set_tests_properties(clientdriver.selftest PROPERTIES
        LABELS "driver"
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

    set(AMBROSE_CLIENTDRIVER_BINARIES "")
    if(TARGET loginserver AND TARGET launcher)
        set(AMBROSE_CLIENTDRIVER_BINARIES --binaries "$<TARGET_FILE_DIR:loginserver>")
    endif()

    file(GLOB AMBROSE_CLIENTDRIVER_SCENARIOS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.json")
    foreach(scenario IN LISTS AMBROSE_CLIENTDRIVER_SCENARIOS)
        get_filename_component(name "${scenario}" NAME_WE)
        add_test(NAME clientdriver.${name}
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/drive.py" run
                --scenario "${scenario}" --label "3.24 ${name}" ${AMBROSE_CLIENTDRIVER_BINARIES})
        set_tests_properties(clientdriver.${name} PROPERTIES
            LABELS "client;driver"
            SKIP_RETURN_CODE 77
            RUN_SERIAL TRUE
            TIMEOUT 1200
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endforeach()
else()
    message(STATUS "Python 3 not found: the client driver tests are not registered")
endif()
