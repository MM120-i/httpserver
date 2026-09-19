# Commands cheat sheet

You don't need to memorize anything. Almost everything goes through `make`:

```bash
make <target>            # e.g. make test
make <target> PRESET=asan   # same target, different build (default PRESET is mac-debug)
```

Think of the workflow in three stages: **configure** (CMake learns your
compiler) → **build** (compile) → **run/test** (execute + check).

## Everyday commands

| Command | What it actually does | When to use it |
|---|---|---|
| `make help` | Prints the short list of targets. | Anytime you forget what's available. |
| `make build` | Runs `cmake --preset mac-debug` then compiles with ninja. Output lands in `build/mac-debug/bin/`. | After editing any source file. |
| `make run` | Builds, then starts `./build/mac-debug/bin/server` in the foreground on port 8000. Stops with Ctrl+C. | To manually try the server with curl/browser. |
| `make client` | Builds, then runs the tiny C client, which sends one `GET /` request and prints the raw reply. The server must already be running (see `make run`). | To test the server without curl. |
| `make test` | Builds, then runs `tests/integration_phase1.sh`: 8 checks over real TCP (status line, Content-Length vs body bytes, exact body, second connection, C client, survival of garbage input, survival of empty connection, sanitizer-log scan). Prints PASS/FAIL per check, exits non-zero on any failure. | The one command that answers "does it still work?". Run after every change. |

## Builds (the `PRESET=` part)

A preset is just a named build configuration: which compiler flags and which
I/O backend. `PRESET` picks the output folder `build/<name>/`, so builds never
overwrite each other.

| Preset | Meaning |
|---|---|
| `mac-debug` (default) | AppleClang, debug info, portable poll() backend. Your daily driver. |
| `mac-release` | Same but optimized (`-O3`-ish). For speed comparisons later. |
| `linux-debug` / `linux-epoll` | Built **inside** the Ubuntu container (see `make linux-build`). `linux-epoll` enables the epoll backend. |
| `asan` / `ubsan` / `tsan` | Debug + Address / UndefinedBehavior / Thread sanitizer. They make the compiler watch your program for memory bugs, undefined behavior, and data races, and print a report if one happens. |

Example: `make build PRESET=mac-release` compiles the optimized binary to
`build/mac-release/bin/server`.

## Sanitizer shortcuts

| Command | What it actually does |
|---|---|
| `make asan` | Builds the `asan` preset, runs the smoke test against it, fails if AddressSanitizer printed anything. |
| `make ubsan` | Same for UndefinedBehaviorSanitizer. |
| `make tsan` | Same for ThreadSanitizer. Only meaningful once threads exist (Phase 6+); until then it just builds and smoke-tests. |

Rule of thumb: if `make test` passes but `make asan` fails, you have a memory
bug that normal runs hide. Fix it before moving on.

## Style checks

| Command | What it actually does |
|---|---|
| `make format` | Rewrites all sources in `src/` and `tests/` to match `.clang-format` (LLVM style) in place. |
| `make format-check` | Same check without rewriting. Exits non-zero (fails) if anything differs. This is what CI will run. |
| `make tidy` | Runs clang-tidy static analysis. Skips gracefully on this Mac (needs full LLVM); enforced on Linux. |

## Linux commands (need Docker Desktop running)

| Command | What it actually does |
|---|---|
| `make linux-build` | Deletes `build/linux-debug` (a Mac-configured dir can't be reused in the container), then configures + compiles with GCC inside Ubuntu 24.04. |
| `make linux-test` | `linux-build`, then runs the Linux server + Linux client inside the container and checks the reply. |

## Debugging

`make debug` doesn't run anything — debuggers are interactive, so it just
prints the recipes: the lldb breakpoint commands for Mac and the gdb-in-Docker
steps for Linux.

## Cleanup

`make clean` deletes the whole `build/` directory. Safe anytime; next build
just reconfigures from scratch. Use it if CMake ever complains about a stale
cache.
