# Project Ambrose by Imjustchico
# Builds libFuzzer targets for frame and message decoding when AMBROSE_BUILD_FUZZERS is on, and runs their seeded ten-minute fuzz passes under CTest with the label fuzz.
add_executable(objectproperty_fuzzer
    "${CMAKE_CURRENT_SOURCE_DIR}/ObjectPropertyFuzzer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../mocks/ObjectFuzzCorpus.cpp")

target_include_directories(objectproperty_fuzzer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../mocks")

target_link_libraries(objectproperty_fuzzer
    PRIVATE
        ambrose-compile-options
        shared
        nlohmann_json::nlohmann_json)

target_link_options(objectproperty_fuzzer PRIVATE -fsanitize=fuzzer)

add_test(NAME ObjectPropertyFuzzer.Corpus
    COMMAND objectproperty_fuzzer "${CMAKE_CURRENT_BINARY_DIR}/objectproperty-corpus" -runs=500000 -seed=1 -max_len=8192 -print_final_stats=1)
set_tests_properties(ObjectPropertyFuzzer.Corpus PROPERTIES LABELS fuzz TIMEOUT 1200)

foreach(target IN ITEMS frame_fuzzer message_decode_fuzzer)
    if(target STREQUAL "frame_fuzzer")
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/FrameFuzz.cpp")
        set(corpus "${CMAKE_CURRENT_BINARY_DIR}/frame-fuzz-corpus")
    else()
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/MessageDecodeFuzz.cpp")
        set(corpus "${CMAKE_CURRENT_BINARY_DIR}/message-decode-fuzz-corpus")
    endif()
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE ambrose-compile-options shared)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    add_test(NAME "${target}.TenMinuteRun"
        COMMAND ${target} "${corpus}" -max_total_time=600 -max_len=4194304 -print_final_stats=1)
    set_tests_properties("${target}.TenMinuteRun" PROPERTIES LABELS fuzz TIMEOUT 1200)
endforeach()
