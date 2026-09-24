# Project Ambrose by Imjustchico
# Registers the codestyle self-tests and the whole-tree style check with CTest when Python 3 is available.
find_package(Python3 COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
    add_test(NAME codestyle.selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_codestyle.py")
    add_test(NAME codestyle.tree
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/codestyle.py" --root "${CMAKE_SOURCE_DIR}")
    set_tests_properties(codestyle.selftest codestyle.tree PROPERTIES
        LABELS "codestyle"
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
else()
    message(STATUS "Python 3 not found: codestyle tests are not registered")
endif()
