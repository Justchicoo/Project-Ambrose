# Project Ambrose by Imjustchico
# Builds unit_tests from every test source outside the compile, client, fuzz, helpers and tools folders, with the launcher's own tests joining it when the launcher library is built so one executable holds them, and outside the login server's and the supervisor's folders unless their libraries are built, together with the child_process_helper program its ChildProcess tests run, which links the common library and whose DLL list the tests get to copy it elsewhere, hands the tests the path of the generated design token stylesheet so the terminal header can be checked against it, links the Windows security API the admin token permission checks read, registers each test with CTest, adds the compile and client folders, the typeextract tests when that tool is built and the fuzz folder when fuzzers are built, and smoke-tests each server executable, the supervisor and the bindecode, dbimport, extractor, launcher, localetool and typeextract tools.
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

ambrose_collect_sources("${CMAKE_CURRENT_SOURCE_DIR}" all_test_sources)
set(test_sources "")
foreach(source IN LISTS all_test_sources)
    file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
    if(relative MATCHES "^(compile|client|fuzz|helpers|tools)/")
        continue()
    endif()
    if(relative MATCHES "^server/apps/loginserver/" AND NOT TARGET login)
        continue()
    endif()
    if(relative MATCHES "^server/apps/supervisor/" AND NOT TARGET supervision)
        continue()
    endif()
    list(APPEND test_sources "${source}")
endforeach()
add_executable(unit_tests ${test_sources})

add_executable(child_process_helper "${CMAKE_CURRENT_SOURCE_DIR}/helpers/ChildProcessHelper.cpp")
target_link_libraries(child_process_helper PRIVATE ambrose-compile-options common)
if(WIN32)
    target_link_libraries(child_process_helper PRIVATE shell32)
endif()
add_dependencies(unit_tests child_process_helper)
target_compile_definitions(unit_tests PRIVATE "AMBROSE_CHILD_PROCESS_HELPER=\"$<TARGET_FILE:child_process_helper>\"")
target_compile_definitions(unit_tests PRIVATE "AMBROSE_CHILD_PROCESS_HELPER_DLLS=\"$<$<BOOL:${WIN32}>:$<JOIN:$<TARGET_RUNTIME_DLLS:child_process_helper>,|>>\"")

target_compile_definitions(unit_tests PRIVATE "AMBROSE_DESIGN_TOKENS_CSS=\"${PROJECT_SOURCE_DIR}/packages/ui/src/tokens/tokens.css\"")

target_include_directories(unit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mocks")

target_link_libraries(unit_tests
    PRIVATE
        ambrose-compile-options
        accounts
        characters
        globals
        scripts
        world
        chat
        nlohmann_json::nlohmann_json
        GTest::gmock)

if(WIN32)
    target_link_libraries(unit_tests PRIVATE advapi32)
endif()

if(TARGET login)
    target_link_libraries(unit_tests PRIVATE login)
endif()

if(TARGET supervision)
    target_link_libraries(unit_tests PRIVATE supervision)
endif()

if(TARGET launcher-core)
    file(GLOB launcher_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/launcher/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/tools/launcher/*.h")
    target_sources(unit_tests PRIVATE ${launcher_test_sources})
    target_include_directories(unit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tools/launcher")
    target_link_libraries(unit_tests PRIVATE launcher-core)
endif()

if(TARGET patchlist_generator)
    target_sources(unit_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/src/tools/patchlist_generator/Core/PatchListGenerator.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/patchlist_generator/PatchListGeneratorTest.cpp")
    target_include_directories(unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/tools/patchlist_generator/Core")
endif()

ambrose_copy_mariadb_plugins(unit_tests)
gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 120)

add_subdirectory(compile)
add_subdirectory(client)
if(TARGET typeextract-core)
    add_subdirectory(tools/typeextract)
endif()
if(AMBROSE_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

foreach(app IN ITEMS gameserver loginserver patchserver supervisor)
    if(TARGET ${app})
        add_test(NAME AppSmoke.${app}
            COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:${app}> -DUNIT_TESTS=$<TARGET_FILE:unit_tests> -DNAME=${app} -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/app-smoke/${app} -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/AppSmokeTest.cmake")
    endif()
endforeach()

if(TARGET bindecode)
    add_test(NAME BinDecode
        COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:bindecode> -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/bindecode -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/BinDecodeTest.cmake")
    set_tests_properties(BinDecode PROPERTIES SKIP_REGULAR_EXPRESSION "bindecode test skipped")
endif()

if(TARGET client)
    add_test(NAME ClientTool
        COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:client> -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/clienttool -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/ClientToolTest.cmake")
    set_tests_properties(ClientTool PROPERTIES SKIP_REGULAR_EXPRESSION "client tool test skipped")
endif()

if(TARGET extractor)
    add_test(NAME Extractor
        COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:extractor> -DDBIMPORT=$<$<TARGET_EXISTS:dbimport>:$<TARGET_FILE:dbimport>> -DGAMESERVER=$<$<TARGET_EXISTS:gameserver>:$<TARGET_FILE:gameserver>> -DUNIT_TESTS=$<TARGET_FILE:unit_tests> -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/extractor -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/ExtractorTest.cmake")
    set_tests_properties(Extractor PROPERTIES SKIP_REGULAR_EXPRESSION "extractor test skipped")
endif()

if(TARGET launcher)
    add_test(NAME Launcher
        COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:launcher> -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/launcher -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/LauncherTest.cmake")
    set_tests_properties(Launcher PROPERTIES SKIP_REGULAR_EXPRESSION "launcher test skipped")
endif()

if(TARGET localetool)
    add_test(NAME LocaleTool
        COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:localetool> -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/localetool -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/LocaleToolTest.cmake")
    set_tests_properties(LocaleTool PROPERTIES SKIP_REGULAR_EXPRESSION "localetool test skipped")
endif()

if(TARGET typeextract)
    add_test(NAME TypeExtract
        COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:typeextract> -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/typeextract -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/TypeExtractTest.cmake")
    set_tests_properties(TypeExtract PROPERTIES SKIP_REGULAR_EXPRESSION "typeextract test skipped")
endif()

if(TARGET dbimport)
    add_test(NAME DbImport
        COMMAND "${CMAKE_COMMAND}" -DAPP=$<TARGET_FILE:dbimport> -DUNIT_TESTS=$<TARGET_FILE:unit_tests> -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/dbimport -DSOURCE=${PROJECT_SOURCE_DIR} -P "${CMAKE_CURRENT_SOURCE_DIR}/apps/DbImportTest.cmake")
    set_tests_properties(DbImport PROPERTIES SKIP_REGULAR_EXPRESSION "dbimport test skipped")
endif()
