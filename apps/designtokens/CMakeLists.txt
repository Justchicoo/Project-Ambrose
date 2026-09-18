# Project Ambrose by Imjustchico
# Registers the design token self-tests and the check that every generated file still matches design/tokens.json with CTest when Python 3 is available.
find_package(Python3 COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
    add_test(NAME designtokens.selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_designtokens.py")
    add_test(NAME designtokens.check
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/designtokens.py" --root "${CMAKE_SOURCE_DIR}" --check)
    set_tests_properties(designtokens.selftest designtokens.check PROPERTIES
        LABELS "designtokens"
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
else()
    message(STATUS "Python 3 not found: design token tests are not registered")
endif()
