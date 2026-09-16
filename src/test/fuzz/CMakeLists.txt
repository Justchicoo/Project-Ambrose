# Project Ambrose by Imjustchico
# Builds the libFuzzer targets for decoders of untrusted data when AMBROSE_BUILD_FUZZERS is on, and runs each for a bounded number of inputs from its seed corpus under CTest with the label fuzz.
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
