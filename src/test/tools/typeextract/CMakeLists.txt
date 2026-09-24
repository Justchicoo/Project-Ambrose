# Project Ambrose by Imjustchico
# Builds typeextract_tests over the typeextract library: unit tests on synthetic PE images, code and heaps that need no client, and the client-gated extraction checks, labelled client, that read the user's own install and type dump.
file(GLOB typeextract_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
add_executable(typeextract_tests ${typeextract_test_sources} "${CMAKE_CURRENT_SOURCE_DIR}/../../Main.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../mocks/LogTestDirectory.cpp")

target_include_directories(typeextract_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../../mocks")

target_link_libraries(typeextract_tests
    PRIVATE
        ambrose-compile-options
        typeextract-core
        nlohmann_json::nlohmann_json
        GTest::gmock)

set(typeextract_client_tests "TypeExtractionClientTest.*")
gtest_discover_tests(typeextract_tests
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 120
    TEST_FILTER "-${typeextract_client_tests}")
gtest_discover_tests(typeextract_tests
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 120
    TEST_FILTER "${typeextract_client_tests}"
    PROPERTIES LABELS client RUN_SERIAL TRUE)
