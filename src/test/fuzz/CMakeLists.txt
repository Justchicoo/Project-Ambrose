# Project Ambrose by Imjustchico
# Builds libFuzzer targets for frame and message decoding when AMBROSE_BUILD_FUZZERS is on, runs their seeded ten-minute fuzz passes under CTest with the label fuzz, and proves their allocation ceiling trips one byte above the frame limit.
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

foreach(probe IN ITEMS at above)
    set(target "allocation_ceiling_${probe}_probe")
    add_executable(${target} "${CMAKE_CURRENT_SOURCE_DIR}/AllocationCeilingProbe.cpp")
    if(probe STREQUAL "at")
        target_compile_definitions(${target} PRIVATE AMBROSE_PROBE_SIZE=FrameLimits::DefaultMaxFrameSize)
    else()
        target_compile_definitions(${target} PRIVATE "AMBROSE_PROBE_SIZE=(FrameLimits::DefaultMaxFrameSize + 1)")
    endif()
    target_link_libraries(${target} PRIVATE ambrose-compile-options shared)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    add_test(NAME "AllocationCeiling.${probe}Limit" COMMAND ${target} -runs=1)
    set_tests_properties("AllocationCeiling.${probe}Limit" PROPERTIES LABELS fuzz TIMEOUT 120)
endforeach()
set_tests_properties("AllocationCeiling.aboveLimit" PROPERTIES PASS_REGULAR_EXPRESSION "deadly signal")
