# Project Ambrose by Imjustchico
# Registers the CI script self-tests, the front-end check self-tests, the forbidden file scan, the front-end repository checks, the contributor track path check and the findings check with CTest when Python 3 is available.
find_package(Python3 COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
    add_test(NAME ci.selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ci.py")
    add_test(NAME ci.forbidden
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ci_forbidden_files.py" --root "${CMAKE_SOURCE_DIR}")
    add_test(NAME ci.contrib
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ci_contrib_paths.py" --root "${CMAKE_SOURCE_DIR}" --paths contrib/notes/sample.md)
    add_test(NAME ci.findings
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ci_findings.py" --root "${CMAKE_SOURCE_DIR}")
    add_test(NAME ci.frontend.selftest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ci_frontend.py")
    add_test(NAME ci.frontend
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ci_frontend_checks.py" --root "${CMAKE_SOURCE_DIR}")
    set_tests_properties(ci.selftest ci.forbidden ci.contrib ci.findings ci.frontend.selftest ci.frontend PROPERTIES
        LABELS "ci"
        ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
else()
    message(STATUS "Python 3 not found: CI script tests are not registered")
endif()
