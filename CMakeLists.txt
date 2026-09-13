# Project Ambrose by Imjustchico
# Root build definition: language standard, build options, dependencies, and source tree.
cmake_minimum_required(VERSION 3.25)

project(Ambrose LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/src/cmake")

option(BUILD_TESTING "Build the unit tests" ON)
option(TOOLS "Build the tools in src/tools" ON)
option(APPS_BUILD "Build the server apps" ON)
option(AMBROSE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(PlatformDetect)
include(CompilerFlags)
include(AmbroseMacros)

find_package(fmt CONFIG REQUIRED)

add_subdirectory(src)
