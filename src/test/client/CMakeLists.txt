# Project Ambrose by Imjustchico
# Builds client_tests, which read the user's own client install and carry the CTest label client.
file(GLOB client_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(client_tests ${client_test_sources} "${CMAKE_CURRENT_SOURCE_DIR}/../Main.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../mocks/MessageRoundTrip.cpp")

target_include_directories(client_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../mocks")

target_link_libraries(client_tests
    PRIVATE
        ambrose-compile-options
        shared
        GTest::gmock)

gtest_discover_tests(client_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS client)
