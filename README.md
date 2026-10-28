# httpserver

A concurrent HTTP/1.1 server written from scratch in C and C++20 on raw POSIX sockets. The project is a systems engineering lab covering networking, HTTP parsing, routing, memory ownership, tests under sanitizers, and continuous integration on macOS and Linux.

Current status: Phase 4 of 28 complete. See `plan.txt` for the full roadmap.

## Features

* C socket layer with an RAII wrapper for socket ownership
* Zero copy HTTP/1.1 parser with strict method, URI, header, and body limits
* Request routing in C++ with exact match dispatch for GET and POST
* HTTP responses with computed content lengths and header injection protection
* Catch2 unit suites and live TCP integration checks
* AddressSanitizer, UndefinedBehaviorSanitizer, and ThreadSanitizer presets
* GitHub Actions matrix covering macOS and Linux with GCC and Clang

## Requirements

CMake 3.20 or newer, Ninja, and a compiler with C11 and C++20 support. Catch2 3 is required for the unit tests. Docker is optional for Linux builds, and curl is needed for integration tests.

Homebrew:

```sh
brew install cmake ninja catch2
```

Ubuntu 24.04:

```sh
sudo apt install cmake ninja-build catch2
```

## Build and Run

```sh
cmake --preset mac-debug
cmake --build build/mac-debug
./build/mac-debug/bin/server
```

The server listens on port 8000.

```sh
curl http://localhost:8000/
```

The Makefile wraps the common workflows:

```sh
make build
make run
```

## Tests

```sh
make unit   # Catch2 suites through CTest
make test   # TCP integration checks
make asan
make ubsan
```

## Linux

Build and test inside an Ubuntu 24.04 container without a local Linux toolchain:

```sh
make linux-build
make linux-test
```

## Routes

| Method | Target | Behavior |
|--------|--------|----------|
| GET | `/` | Hello, world! |
| GET | `/status` | JSON status document |
| GET | `/users` | JSON user list |
| POST | `/users` | 201 Created with echoed body |

## Layout

```text
src/
  main.cpp          server loop
  net/              socket syscalls and RAII wrapper
  http/             parser, request, response
  routing/          router
tests/              Catch2 suites and integration script
.github/workflows/  CI matrix
```

## Continuous Integration

Every push and pull request builds and tests on macOS with AppleClang and on Linux with GCC and Clang. Linux jobs also exercise the sanitizer presets. Formatting and static analysis jobs join once the codebase is formatted.