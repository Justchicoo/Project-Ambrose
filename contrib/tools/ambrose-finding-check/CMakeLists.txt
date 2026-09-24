# Project Ambrose by Imjustchico
# Builds the dependency-free machine-checkable finding validator.

cmake_minimum_required(VERSION 3.25)

project(ambrose_finding_check LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-finding-check
    src/main.cpp
)
