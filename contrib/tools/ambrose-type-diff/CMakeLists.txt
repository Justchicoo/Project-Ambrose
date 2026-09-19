# Project Ambrose by Imjustchico
# Builds the standalone Ambrose type-dump comparison tool.

cmake_minimum_required(VERSION 3.25)

project(ambrose_type_diff LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-type-diff
    src/main.cpp
)
