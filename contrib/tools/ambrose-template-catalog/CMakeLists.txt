# Project Ambrose by Imjustchico
# Builds the template cataloger against the existing debug libraries and dependencies.

cmake_minimum_required(VERSION 3.25)

project(ambrose_template_catalog LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AMBROSE_ROOT "" CACHE PATH "Project Ambrose source directory")
set(AMBROSE_BUILD_DIR "" CACHE PATH "Configured Project Ambrose build directory")
set(AMBROSE_VCPKG_TRIPLET "x64-windows" CACHE STRING "Triplet used by the Project Ambrose build")

if(NOT IS_DIRECTORY "${AMBROSE_ROOT}/src")
    message(FATAL_ERROR "Set AMBROSE_ROOT to the Project Ambrose source directory")
endif()
if(NOT IS_DIRECTORY "${AMBROSE_BUILD_DIR}/src/server/game/Entities")
    message(FATAL_ERROR "Set AMBROSE_BUILD_DIR to a configured Project Ambrose build directory")
endif()

set(AMBROSE_VCPKG "${AMBROSE_BUILD_DIR}/vcpkg_installed/${AMBROSE_VCPKG_TRIPLET}")
set(AMBROSE_DEBUG_LIB "${AMBROSE_VCPKG}/debug/lib")

set(AMBROSE_LIBRARIES
    "${AMBROSE_BUILD_DIR}/src/server/game/Entities/Debug/entities.lib"
    "${AMBROSE_BUILD_DIR}/src/server/game/Characters/Debug/characters.lib"
    "${AMBROSE_BUILD_DIR}/src/server/database/Debug/database.lib"
    "${AMBROSE_BUILD_DIR}/src/server/game/Globals/Debug/globals.lib"
    "${AMBROSE_BUILD_DIR}/src/server/game/Movement/Debug/movement.lib"
    "${AMBROSE_BUILD_DIR}/src/server/shared/Debug/shared.lib"
    "${AMBROSE_BUILD_DIR}/src/common/Debug/common.lib"
    "${AMBROSE_DEBUG_LIB}/libmariadb.lib"
    "${AMBROSE_DEBUG_LIB}/fmtd.lib"
    "${AMBROSE_VCPKG}/lib/botan-3.lib"
    "${AMBROSE_DEBUG_LIB}/zd.lib"
    "${AMBROSE_DEBUG_LIB}/pugixml.lib"
    "${AMBROSE_DEBUG_LIB}/libssl.lib"
    "${AMBROSE_DEBUG_LIB}/libcrypto.lib"
    "${AMBROSE_DEBUG_LIB}/ftxui-component.lib"
    "${AMBROSE_DEBUG_LIB}/ftxui-dom.lib"
    "${AMBROSE_DEBUG_LIB}/ftxui-screen.lib"
)

foreach(library IN LISTS AMBROSE_LIBRARIES)
    if(NOT EXISTS "${library}")
        message(FATAL_ERROR "Required Project Ambrose library is missing: ${library}")
    endif()
endforeach()

file(GLOB_RECURSE AMBROSE_INCLUDE_DIRS LIST_DIRECTORIES true
    "${AMBROSE_ROOT}/src/common/*"
    "${AMBROSE_ROOT}/src/server/*")
list(APPEND AMBROSE_INCLUDE_DIRS
    "${AMBROSE_ROOT}/src/common"
    "${AMBROSE_ROOT}/src/server/shared"
    "${AMBROSE_ROOT}/src/server/game/Entities"
    "${AMBROSE_ROOT}/src/server/game/Characters"
    "${AMBROSE_ROOT}/src/server/database"
    "${AMBROSE_ROOT}/src/server/game/Globals"
    "${AMBROSE_ROOT}/src/server/game/Movement"
    "${AMBROSE_VCPKG}/include")

file(TO_CMAKE_PATH "${AMBROSE_ROOT}" AMBROSE_ROOT_CMAKE_PATH)

add_executable(ambrose-template-catalog main.cpp)
target_include_directories(ambrose-template-catalog PRIVATE ${AMBROSE_INCLUDE_DIRS})
target_compile_options(ambrose-template-catalog PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
target_compile_definitions(ambrose-template-catalog PRIVATE
    _WIN32_WINNT=0x0A00
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    "AMBROSE_SOURCE_DIR=\"${AMBROSE_ROOT_CMAKE_PATH}\"")
target_link_libraries(ambrose-template-catalog PRIVATE ${AMBROSE_LIBRARIES} shell32 advapi32)
