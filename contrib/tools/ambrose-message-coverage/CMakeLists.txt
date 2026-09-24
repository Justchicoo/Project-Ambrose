# Project Ambrose by Imjustchico
# Builds the standalone C-24 message coverage tool.

cmake_minimum_required(VERSION 3.25)

project(ambrose_message_coverage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-message-coverage
    src/main.cpp
)
