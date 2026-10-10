PRESET    ?= mac-debug
BUILD_DIR := build/$(PRESET)
SERVER    := $(BUILD_DIR)/bin/server
CLIENT    := $(BUILD_DIR)/bin/client
PORT      := 8000

SOURCES := $(shell find src tests -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) 2>/dev/null)

.DEFAULT_GOAL := help
.PHONY: help configure build rebuild run client test unit asan ubsan tsan \
        format format-check tidy clean linux-build linux-test debug

help: 
	@echo "Targets (append PRESET=<name> to override, default: mac-debug):"
	@echo "  build        configure + compile              (PRESETs: mac-debug mac-release linux-debug linux-epoll asan ubsan tsan)"
	@echo "  run          build + run server (Ctrl+C stops, port $(PORT))"
	@echo "  client       run the C client (needs server up)"
	@echo "  test         build + Phase 1 integration tests (8 checks over TCP)"
	@echo "  unit         build + Catch2 unit tests via CTest"
	@echo "  asan/ubsan/tsan  build with sanitizer + integration tests"
	@echo "  tidy         clang-tidy (skips gracefully if not installed)"
	@echo "  linux-build  configure + compile inside Ubuntu 24.04 container"
	@echo "  linux-test   linux-build + run server+client inside container"
	@echo "  debug        print lldb/gdb breakpoint recipes"
	@echo "  clean        rm -rf build/"

configure:  
	cmake --preset $(PRESET)

build: configure 
	cmake --build $(BUILD_DIR)

rebuild: 
	rm -rf $(BUILD_DIR)
	$(MAKE) build PRESET=$(PRESET)

run: build 
	$(SERVER)

client: build 
	$(CLIENT)

test: build
	bash tests/integration_phase1.sh $(SERVER) $(CLIENT) $(PORT)

unit: build 
	ctest --test-dir $(BUILD_DIR) --output-on-failure 
	
asan: 
	@$(MAKE) build PRESET=asan
	bash tests/integration_phase1.sh build/asan/bin/server build/asan/bin/client $(PORT)

ubsan: 
	@$(MAKE) build PRESET=ubsan
	bash tests/integration_phase1.sh build/ubsan/bin/server build/ubsan/bin/client $(PORT)

tsan: 
	@$(MAKE) build PRESET=tsan
	bash tests/integration_phase1.sh build/tsan/bin/server build/tsan/bin/client $(PORT)

tidy: build 
	@command -v clang-tidy >/dev/null 2>&1 || { echo "clang-tidy not installed (needs full LLVM) — skipping"; exit 0; }
	@test -f $(BUILD_DIR)/compile_commands.json || { echo "no compile_commands.json — add CMAKE_EXPORT_COMPILE_COMMANDS=ON to use tidy"; exit 1; }
	clang-tidy -p $(BUILD_DIR) src/main.cpp src/net/socket.c src/client.c

clean: 
	rm -rf build

linux-build: 
	@docker info >/dev/null 2>&1 || { echo "Docker daemon is down — open Docker Desktop first"; exit 1; }
	rm -rf build/linux-debug
	docker run --rm -v "$(PWD):/work" -w /work ubuntu:24.04 bash -c \
		"apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq gcc g++ cmake ninja-build catch2 >/dev/null 2>&1 && cmake --preset linux-debug && cmake --build build/linux-debug"

linux-test: linux-build 
	docker run --rm -v "$(PWD):/work" -w /work ubuntu:24.04 bash -c \
		"./build/linux-debug/bin/server > /tmp/srv.txt 2>&1 & SRV=\$$!; sleep 2; ./build/linux-debug/bin/client; RC=\$$?; kill \$$SRV; exit \$$RC"

debug: 
	@echo "lldb (Mac):"
	@echo "  lldb ./build/mac-debug/bin/server"
	@echo "  (lldb) breakpoint set --name tcp_accept"
	@echo "  (lldb) run   # then curl in another terminal, then: bt / continue"
	@echo "gdb (Linux container, needs Docker daemon):"
	@echo "  docker run --rm -it -v \"\$$(PWD):/work\" -w /work -p $(PORT):$(PORT) ubuntu:24.04 bash"
	@echo "  # inside: apt install gcc g++ cmake ninja-build gdb, cmake --preset linux-debug,"
	@echo "  #   cmake --build build/linux-debug, gdb ./build/linux-debug/bin/server"
	@echo "  #   (gdb) break tcp_accept  /  run  /  backtrace  /  continue"
