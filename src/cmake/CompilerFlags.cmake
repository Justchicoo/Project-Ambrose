# Project Ambrose by Imjustchico
# Defines the ambrose-compile-options target that carries warnings and platform defines for project code, and under Visual Studio's generator compiles a project's files on every core, which MSBuild's own parallelism across projects does not; the source root reaches the code as a define only under MSVC, since GCC and Clang trim it from __FILE__ with a prefix map, so their command lines name no clone and a compiler cache can share objects between clones.
add_library(ambrose-compile-options INTERFACE)

target_compile_features(ambrose-compile-options INTERFACE cxx_std_20)

if(MSVC)
    target_compile_definitions(ambrose-compile-options INTERFACE AMBROSE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/")
    target_compile_options(ambrose-compile-options INTERFACE
        /W4
        /permissive-
        /utf-8
        /Zc:__cplusplus
        /EHsc
        /external:anglebrackets
        /external:W0)
    target_compile_definitions(ambrose-compile-options INTERFACE
        _WIN32_WINNT=0x0A00
        NOMINMAX
        WIN32_LEAN_AND_MEAN)
    if(AMBROSE_WARNINGS_AS_ERRORS)
        target_compile_options(ambrose-compile-options INTERFACE /WX)
    endif()
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        target_compile_options(ambrose-compile-options INTERFACE /MP)
    endif()
else()
    target_compile_options(ambrose-compile-options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        "-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=")
    if(AMBROSE_WARNINGS_AS_ERRORS)
        target_compile_options(ambrose-compile-options INTERFACE -Werror)
    endif()
endif()

if(AMBROSE_SANITIZE_ADDRESS AND AMBROSE_SANITIZE_THREAD)
    message(FATAL_ERROR "AMBROSE_SANITIZE_ADDRESS and AMBROSE_SANITIZE_THREAD cannot be combined")
endif()

if(AMBROSE_SANITIZE_THREAD)
    if(MSVC)
        message(FATAL_ERROR "ThreadSanitizer is not available with MSVC; use the linux-clang-tsan preset")
    endif()
    target_compile_definitions(ambrose-compile-options INTERFACE AMBROSE_SANITIZE_THREAD)
    target_compile_options(ambrose-compile-options INTERFACE
        -fsanitize=thread
        -fno-omit-frame-pointer)
    target_link_options(ambrose-compile-options INTERFACE -fsanitize=thread)
endif()

if(AMBROSE_BUILD_FUZZERS)
    if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "AMBROSE_BUILD_FUZZERS needs Clang's libFuzzer; use the linux-clang-fuzz preset")
    endif()
    if(AMBROSE_SANITIZE_THREAD)
        message(FATAL_ERROR "AMBROSE_BUILD_FUZZERS cannot be combined with AMBROSE_SANITIZE_THREAD")
    endif()
    target_compile_definitions(ambrose-compile-options INTERFACE AMBROSE_BUILD_FUZZERS)
    target_compile_options(ambrose-compile-options INTERFACE -fsanitize=fuzzer-no-link)
    target_link_options(ambrose-compile-options INTERFACE -fsanitize=fuzzer-no-link)
endif()

if(AMBROSE_SANITIZE_ADDRESS)
    target_compile_definitions(ambrose-compile-options INTERFACE AMBROSE_SANITIZE_ADDRESS)
    if(MSVC)
        target_compile_options(ambrose-compile-options INTERFACE /fsanitize=address)
    else()
        target_compile_options(ambrose-compile-options INTERFACE
            -fsanitize=address,undefined
            -fno-sanitize-recover=all
            -fno-omit-frame-pointer)
        target_link_options(ambrose-compile-options INTERFACE -fsanitize=address,undefined)
    endif()
endif()
