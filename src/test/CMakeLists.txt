# Project Ambrose by Imjustchico
# Builds unit_tests from every test source outside the compile, client and fuzz folders, and outside the login server's folder unless its library is built, registers each test with CTest, adds the compile and client folders and the fuzz folder when fuzzers are built, and smoke-tests each server executable.
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" all_test_sources)
set(test_sources "")
foreach(source IN LISTS all_test_sources)
    file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
    if(relative MATCHES "^(compile|client|fuzz)/")
        continue()
    endif()
    if(relative MATCHES "^server/apps/loginserver/" AND NOT TARGET login)
        continue()
    endif()
    list(APPEND test_sources "${source}")
endforeach()
add_executable(unit_tests ${test_sources})

target_include_directories(unit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mocks")

target_link_libraries(unit_tests
    PRIVATE
        ambrose-compile-options
        accounts
        characters
        globals
        nlohmann_json::nlohmann_json
        GTest::gmock)

if(TARGET login)
    target_link_libraries(unit_tests PRIVATE login)
endif()

ambrose_copy_mariadb_plugins(unit_tests)
gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 120)

add_subdirectory(compile)
add_subdirectory(client)
if(AMBROSE_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

foreach(app IN ITEMS gameserver loginserver patchserver)
    if(TARGET ${app})
        add_test(NAME AppSmoke.${app}
            COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:${app}> -DUNIT_TESTS=$<TARGET_FILE:unit_tests> -DNAME=${app} -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/app-smoke/${app} -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/AppSmokeTest.cmake")
    endif()
endforeach()

if(TARGET dbimport)
    add_test(NAME DbImport
        COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:dbimport> -DUNIT_TESTS=$<TARGET_FILE:unit_tests> -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/dbimport -DSOURCE=${PROJECT_SOURCE_DIR} -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/DbImportTest.cmake")
    set_tests_properties(DbImport PROPERTIES SKIP_REGULAR_EXPRESSION "dbimport test skipped")
endif()
