# Project Ambrose by Imjustchico
# Registers the logging format compile checks and their positive control with CTest.
set(log_format_cases control too_few_arguments wrong_specifier_type malformed_format runtime_format_string runtime_category)
list(LENGTH log_format_cases log_format_count)
math(EXPR log_format_last "${log_format_count} - 1")

foreach(index RANGE ${log_format_last})
    list(GET log_format_cases ${index} name)
    set(target "log_format_check_${name}")
    add_library(${target} OBJECT EXCLUDE_FROM_ALL "${CMAKE_CURRENT_SOURCE_DIR}/Logging/LogFormatCheck.cpp")
    set_target_properties(${target} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    target_compile_definitions(${target} PRIVATE "AMBROSE_LOG_FORMAT_CASE=${index}")
    target_include_directories(${target} PRIVATE "$<TARGET_PROPERTY:common,INTERFACE_INCLUDE_DIRECTORIES>")
    target_link_libraries(${target} PRIVATE ambrose-compile-options fmt::fmt)

    math(EXPR line "1000 + ${index}")
    if(index EQUAL 0)
        set(expect "success")
    else()
        set(expect "failure")
    endif()

    add_test(NAME "logging.compile.${name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DCONFIG=$<CONFIG>"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DTARGET=${target}"
            "-DEXPECT=${expect}"
            "-DLINE=${line}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/RunCompileCheck.cmake")
    set_tests_properties("logging.compile.${name}" PROPERTIES
        LABELS "compile"
        RESOURCE_LOCK "ambrose-build-tree"
        TIMEOUT 900
        ENVIRONMENT "MSBUILDDISABLENODEREUSE=1")
    if(index EQUAL 0)
        set_tests_properties("logging.compile.${name}" PROPERTIES FIXTURES_SETUP "LogFormatControl")
    else()
        set_tests_properties("logging.compile.${name}" PROPERTIES FIXTURES_REQUIRED "LogFormatControl")
    endif()
endforeach()
