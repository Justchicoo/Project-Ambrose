# Project Ambrose by Imjustchico
# Builds launcher_tests over the launcher library: discovery, the generated client configuration, the run folder, the argument list, the log follower and every refusal, all on a described machine with an in-memory run folder, so no client is started.
file(GLOB launcher_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
add_executable(launcher_tests ${launcher_test_sources} "${CMAKE_CURRENT_SOURCE_DIR}/../../Main.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../mocks/LogTestDirectory.cpp")

target_include_directories(launcher_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../../mocks")

target_link_libraries(launcher_tests
    PRIVATE
        ambrose-compile-options
        launcher-core
        GTest::gmock)

gtest_discover_tests(launcher_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 120)
