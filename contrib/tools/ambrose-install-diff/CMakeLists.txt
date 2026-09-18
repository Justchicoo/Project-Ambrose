# Project Ambrose by Imjustchico
# Builds the standalone Ambrose installation revision diff tool.

cmake_minimum_required(VERSION 3.25)

project(ambrose_install_diff LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-install-diff
    src/main.cpp
)
