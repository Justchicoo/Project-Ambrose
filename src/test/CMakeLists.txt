# Project Ambrose by Imjustchico
# Builds unit_tests from every test source outside the compile and client folders, registers each test with CTest, adds those two folders, and smoke-tests each server executable.
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" all_test_sources)
set(test_sources "")
foreach(source IN LISTS all_test_sources)
    file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
    if(NOT relative MATCHES "^(compile|client)/")
        list(APPEND test_sources "${source}")
    endif()
endforeach()
add_executable(unit_tests ${test_sources})

target_include_directories(unit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mocks")

target_link_libraries(unit_tests
    PRIVATE
        ambrose-compile-options
        shared
        GTest::gmock)

gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST)

add_subdirectory(compile)
add_subdirectory(client)

foreach(app IN ITEMS gameserver loginserver patchserver)
    if(TARGET ${app})
        add_test(NAME AppSmoke.${app}
            COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:${app}> -DNAME=${app} -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/app-smoke/${app} -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/AppSmokeTest.cmake")
    endif()
endforeach()
