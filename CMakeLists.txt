# Project Ambrose by Imjustchico
# Root build definition: language standard, build options, dependencies, and source tree.
cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_CXX_SCAN_FOR_MODULES)

if(EXISTS "${CMAKE_SOURCE_DIR}/conf/config.cmake")
    include("${CMAKE_SOURCE_DIR}/conf/config.cmake")
endif()

option(BUILD_TESTING "Build the unit tests" ON)
if(BUILD_TESTING)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

project(Ambrose LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/src/cmake")

option(TOOLS "Build the tools in src/tools" ON)
option(APPS_BUILD "Build the server apps" ON)
option(AMBROSE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(AMBROSE_SANITIZE_ADDRESS "Build project code with AddressSanitizer, plus UndefinedBehaviorSanitizer where supported" OFF)
option(AMBROSE_SANITIZE_THREAD "Build project code with ThreadSanitizer (GCC or Clang only)" OFF)
option(AMBROSE_BUILD_FUZZERS "Build libFuzzer targets for decoders of untrusted data, with coverage instrumentation on project code (Clang only)" OFF)

include(PlatformDetect)
include(CompilerFlags)
include(AmbroseMacros)

find_package(fmt CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(Botan 3 CONFIG REQUIRED COMPONENTS sha2_32 sha2_64 hmac system_rng twofish)
find_package(ZLIB REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-libmariadb CONFIG REQUIRED)
find_package(Threads REQUIRED)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(apps/codestyle)
    add_subdirectory(apps/ci)
endif()

add_subdirectory(src)
ambrose_install_runtime_dependencies()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/sql" DESTINATION share/ambrose/data PATTERN ".gitkeep" EXCLUDE)
