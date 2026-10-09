cmake_minimum_required(VERSION 3.20)

project(httpserver LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Exact compile flags for IDEs (clangd/cpptools) via compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_EPOLL "Use Linux epoll backend instead of portable poll()" OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(STATUS "C compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CXX compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "USE_EPOLL: ${USE_EPOLL}")

add_executable(server src/main.cpp src/net/socket.c src/http/parser.c)
target_include_directories(server PRIVATE src)

add_executable(client src/client.c)

find_package(Catch2 3 REQUIRED)

add_executable(parser_tests tests/test_parser.cpp src/http/parser.c)
target_include_directories(parser_tests PRIVATE src)
target_link_libraries(parser_tests PRIVATE Catch2::Catch2WithMain)

enable_testing()
add_test(NAME parser_tests COMMAND parser_tests)