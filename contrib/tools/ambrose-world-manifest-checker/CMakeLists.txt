# Project Ambrose by Imjustchico
# Builds the dependency-free canonical world-manifest checker.

cmake_minimum_required(VERSION 3.25)

project(ambrose_world_manifest_checker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-world-manifest-checker
    src/main.cpp
)
