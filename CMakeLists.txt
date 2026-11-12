cmake_minimum_required(VERSION 3.20)

project(httpserver LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_EPOLL "Use Linux epoll backend instead of portable poll()" OFF)

if(USE_EPOLL)
    add_compile_definitions(USE_EPOLL=1)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Werror)
find_package(Threads REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(STATUS "C compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CXX compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "USE_EPOLL: ${USE_EPOLL}")

add_executable(server src/main.cpp src/net/socket.c src/net/poller.cpp src/http/parser.c src/http/response.cpp src/routing/router.cpp src/concurrency/thread_pool.cpp)
target_include_directories(server PRIVATE src)
target_link_libraries(server PRIVATE Threads::Threads)

add_executable(client src/client.c)
find_package(Catch2 3 REQUIRED)

add_executable(parser_tests tests/test_parser.cpp src/http/parser.c)
target_include_directories(parser_tests PRIVATE src)
target_link_libraries(parser_tests PRIVATE Catch2::Catch2WithMain)

enable_testing()

add_test(NAME parser_tests COMMAND parser_tests)

add_executable(router_tests tests/test_router.cpp src/http/response.cpp src/routing/router.cpp)
target_include_directories(router_tests PRIVATE src)
target_link_libraries(router_tests PRIVATE Catch2::Catch2WithMain)
add_test(NAME router_tests COMMAND router_tests)

add_executable(thread_pool_tests tests/test_thread_pool.cpp src/concurrency/thread_pool.cpp)
target_include_directories(thread_pool_tests PRIVATE src)
target_link_libraries(thread_pool_tests PRIVATE Catch2::Catch2WithMain Threads::Threads)
add_test(NAME thread_pool_tests COMMAND thread_pool_tests)

add_executable(race_demo tests/race_demo.cpp)
target_link_libraries(race_demo PRIVATE Threads::Threads)

add_executable(socket_io_tests tests/test_socket_io.cpp src/net/socket.c)
target_include_directories(socket_io_tests PRIVATE src)
target_link_libraries(socket_io_tests PRIVATE Catch2::Catch2WithMain)
add_test(NAME socket_io_tests COMMAND socket_io_tests)

add_executable(poller_tests tests/test_poller.cpp src/net/poller.cpp src/net/socket.c)
target_include_directories(poller_tests PRIVATE src)
target_link_libraries(poller_tests PRIVATE Catch2::Catch2WithMain)
add_test(NAME poller_tests COMMAND poller_tests)
