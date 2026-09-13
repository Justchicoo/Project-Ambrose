# Project Ambrose by Imjustchico
# Builds the single unit_tests executable from every test source and registers each test with CTest.
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" test_sources)
list(FILTER test_sources EXCLUDE REGEX "/compile/")
add_executable(unit_tests ${test_sources})

target_include_directories(unit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mocks")

target_link_libraries(unit_tests
    PRIVATE
        ambrose-compile-options
        shared
        GTest::gmock)

gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST)

add_subdirectory(compile)
