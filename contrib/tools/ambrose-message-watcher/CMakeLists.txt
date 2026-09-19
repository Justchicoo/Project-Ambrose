# Project Ambrose by Imjustchico
# Builds the standalone Ambrose refused-message watcher.

cmake_minimum_required(VERSION 3.25)

project(ambrose_message_watcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ambrose-message-watcher
    src/main.cpp
)
