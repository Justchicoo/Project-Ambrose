# Project Ambrose by Imjustchico
# Builds the bounded, loopback-first TCP load generator.

cmake_minimum_required(VERSION 3.25)

project(ambrose_load_generator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-load-generator
    src/main.cpp
)

if (WIN32)
    target_link_libraries(ambrose-load-generator PRIVATE ws2_32)
endif()
