# Project Ambrose by Imjustchico
# Builds the dependency-free metadata-only pcapng capture decoder.

cmake_minimum_required(VERSION 3.25)

project(ambrose_capture_decoder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-capture-decoder
    src/main.cpp
)
