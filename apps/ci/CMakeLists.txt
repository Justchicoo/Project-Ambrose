# Project Ambrose by Imjustchico
# Registers the CI script self-tests and the forbidden file scan with CTest when Python 3 is available.
find_package(Python3 COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
    add_test(NAME ci.selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ci.py")
    add_test(NAME ci.forbidden
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ci_forbidden_files.py" --root "${CMAKE_SOURCE_DIR}")
    set_tests_properties(ci.selftest ci.forbidden PROPERTIES
        LABELS "ci"
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
else()
    message(STATUS "Python 3 not found: CI script tests are not registered")
endif()
