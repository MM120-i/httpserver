cmake_minimum_required(VERSION 3.20)

# Phase 0 only: toolchain check. Sources are added in Phase 1+.
project(httpserver LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# I/O backend selection. OFF = portable poll() (Mac + Linux).
# ON = Linux epoll backend (only meaningful on Linux; guarded in Phase 9).
option(USE_EPOLL "Use Linux epoll backend instead of portable poll()" OFF)

# Strict warnings from day 1. -Werror keeps the build green.
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# Output dirs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(STATUS "C compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CXX compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "USE_EPOLL: ${USE_EPOLL}")
