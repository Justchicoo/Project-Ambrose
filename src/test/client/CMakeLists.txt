# Project Ambrose by Imjustchico
# Builds client_tests, which read the user's own client install or type dump and carry the CTest label client, including the game message table check and the login message table check when the login library is built, with the MariaDB client plugins beside it for database checks, and runs the whole-archive sweep on its own because it uses every hardware thread.
file(GLOB client_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if(NOT TARGET login)
    list(FILTER client_test_sources EXCLUDE REGEX "LoginMessageTableClientTest")
endif()
add_executable(client_tests ${client_test_sources} "${CMAKE_CURRENT_SOURCE_DIR}/../Main.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../mocks/MessageRoundTrip.cpp")

target_include_directories(client_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../mocks")

target_link_libraries(client_tests
    PRIVATE
        ambrose-compile-options
        shared
        characters
        gameserver-sessions
        nlohmann_json::nlohmann_json
        GTest::gmock)

if(TARGET login)
    target_link_libraries(client_tests PRIVATE login)
endif()
ambrose_copy_mariadb_plugins(client_tests)

set(client_serial_tests "BindFileClientTest.EveryRootWadBindFileDecodesOrNamesItsUnknownRootClass")
gtest_discover_tests(client_tests
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 120
    TEST_FILTER "-${client_serial_tests}"
    PROPERTIES LABELS client)
gtest_discover_tests(client_tests
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 120
    TEST_FILTER "${client_serial_tests}"
    PROPERTIES LABELS client RUN_SERIAL TRUE)
