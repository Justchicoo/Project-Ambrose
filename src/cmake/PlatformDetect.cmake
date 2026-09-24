# Project Ambrose by Imjustchico
# Detects the target platform and rejects unsupported or 32-bit builds.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Project Ambrose requires a 64-bit build")
endif()

if(WIN32)
    set(AMBROSE_PLATFORM "Windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(AMBROSE_PLATFORM "Linux")
elseif(APPLE)
    set(AMBROSE_PLATFORM "macOS")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

message(STATUS "Project Ambrose platform: ${AMBROSE_PLATFORM}")
