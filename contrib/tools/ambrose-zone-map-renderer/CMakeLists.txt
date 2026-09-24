# Project Ambrose by Imjustchico
# Builds the dependency-free SVG zone-object map renderer.

cmake_minimum_required(VERSION 3.25)

project(ambrose_zone_map_renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-zone-map-renderer
    src/main.cpp
)
