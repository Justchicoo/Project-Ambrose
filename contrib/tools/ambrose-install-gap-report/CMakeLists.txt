# Project Ambrose by Imjustchico
# Builds the dependency-free type-dump and source-reference gap reporter.

cmake_minimum_required(VERSION 3.25)

project(ambrose_install_gap_report LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-install-gap-report
    src/main.cpp
)
