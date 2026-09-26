# Contributing

Read [ARCHITECTURE.md](ARCHITECTURE.md) first if you have not. This document is
about the day to day mechanics: how to build it, what has to be true before a
change lands, and what the reviewers will be looking for.

## Prerequisites

- CMake 3.28 or newer
- Ninja, or any other generator you like
- A C++20 compiler. Clang is what the project is developed against and what CI
  uses; GCC mostly works but is stricter about `-Wconversion` and `-Werror`.
- Clang 22 or newer, for `clang-format`, `clang-tidy` and `llvm-cov`
- `uv`, for `cpplint` and the license header script
- `libunwind` development headers, only for `FPAG_ENABLE_LIBUNWIND=ON`

Nix users can get all of it from the flake:

```bash
nix develop
```

Everything else is fetched by CMake. There is no submodule to initialize and no
package to install before a first build.

## Workflow

```bash
# Debug build with tests and benchmarks, the configuration you want while
# working on something.
cmake --preset dev
cmake --build --preset dev

# Run the tests. One ctest entry per test case, so a failure names the case.
ctest --preset dev

# Everything CI runs, before you push.
ctest --preset asan
cmake --build build/asan --target coverage
cmake --build build/tidy --target tidy
cmake --build build/tidy --target lint
```

`cmake --list-presets` prints every preset with its description. The ones worth
knowing by heart:

| Preset | What it is for |
| --- | --- |
| `default` | RelWithDebInfo with tests. The everyday build |
| `debug` | Debug, with `FPAG_DCHECK` and the frame pointers stack traces need |
| `release` | Release, `-O3`, symbols hidden |
| `dev` | Debug with tests **and** benchmarks |
| `asan` | Debug with the address, leak and undefined sanitizers |
| `coverage` | Debug instrumented for `llvm-cov` |
| `tidy` | Debug with `clang-tidy` as the compiler launcher |
| `native` | Release tuned for this machine. Not reproducible, never for CI |

### Developer targets

`cmake` is the only tool you need:

```bash
cmake --build build/dev --target format        # rewrite the sources in place
cmake --build build/dev --target format-check  # report, do not rewrite
cmake --build build/dev --target tidy          # clang-tidy --fix, then format
cmake --build build/dev --target cpplint       # cpplint, recursively
cmake --build build/dev --target lint          # format-check plus cpplint
cmake --build build/dev --target license       # apply the license header
```

`format` and `tidy` modify files. Run them before you commit, not after.

### Tooling is authoritative

`.clang-format`, `.clang-tidy`, `CPPLINT.cfg` and `typos.toml` define the
project's style. Do not hand-format around them and do not suppress a
diagnostic you have not understood. When the tools and your taste disagree,
the tools win, and the fix belongs in the config file where it is visible in
review.

`clang-tidy` is scoped to fpag's own translation units. It deliberately does not
lint the public headers under `include/`, because doing so surfaces a large
backlog of `misc-const-correctness` findings in code that has been correct so
far. Widening that scope is a change worth making on its own, not as a
side effect of something else.

### clangd

`.clangd` points at `build/dev`. Configure that preset once and the
intellisense, the include resolution and the inline diagnostics all work:

```bash
cmake --preset dev
```

## Build options

| Option | Default | Effect |
| --- | --- | --- |
| `FPAG_BUILD_TESTS` | `ON` when top level | Build the Catch2 test binary |
| `FPAG_BUILD_BENCHMARKS` | `OFF` | Build the Google Benchmark binary |
| `FPAG_INSTALL` | `ON` when top level | Generate install and package config rules |
| `FPAG_ENABLE_SANITIZERS` | `OFF` | `-fsanitize=address,leak,undefined` |
| `FPAG_ENABLE_COVERAGE` | `OFF` | `-fprofile-instr-generate`, adds a `coverage` target |
| `FPAG_ENABLE_CLANG_TIDY` | `OFF` | `clang-tidy` as the compiler launcher |
| `FPAG_ENABLE_LIBUNWIND` | `OFF` | Stack traces through libunwind. Linux only |
| `FPAG_ENABLE_LTO` | `OFF` | Interprocedural optimization |
| `FPAG_ENABLE_UNITY_BUILD` | `OFF` | Merge translation units |
| `FPAG_ENABLE_TIME_TRACE` | `OFF` | `-ftime-trace` |
| `FPAG_ENABLE_XRAY` | `OFF` | `-fxray-instrument` |
| `FPAG_ENABLE_OPT_REPORT` | `OFF` | `-fsave-optimization-record` |
| `FPAG_ENABLE_NATIVE` | `OFF` | `-march=native`, skipped on one core |
| `FPAG_WARNINGS_AS_ERRORS` | `ON` | `-Werror` |
| `FPAG_CXX_STDLIB` | empty | Alternate standard library, e.g. `libc++` |

`FPAG_ENABLE_SANITIZERS` and `FPAG_ENABLE_COVERAGE` are mutually exclusive and
the configure step says so rather than letting the flags fight.

### Dependencies

`fmt` and `xxhash` appear in fpag's public headers, so they are part of the
installed interface. Everything else is private or test only.

Each one is looked for on the machine first and built from a pinned tag if it is
not there:

| Dependency | Used by | Built from source |
| --- | --- | --- |
| fmt 12.2.0 | public headers, library | when not installed |
| xxhash 0.8.3 | `fpag/hash/xxh3_hasher.h` | when not installed |
| libunwind 1.8.3 | stack traces, Linux only | never, install it |
| Catch2 3.15.2 | tests | when not installed |
| Google Benchmark 1.9.5 | benchmarks | when not installed |

xxhash is header only as fpag uses it: `xxh3.h` defines `XXH_INLINE_ALL` before
including `xxhash.h`, so no xxhash library is ever linked. Only the header has
to be reachable.

A dependency this project built itself goes into the install export, together
with its headers, so the installed tree is self contained. A dependency that was
found is resolved by `find_dependency` in the generated package config instead.

**`FPAG_CXX_STDLIB` is the one setting that can break the build in a way that
does not look like a build error.** `libfpag` is static and records nothing
about the standard library it was compiled against, so:

- A dependency this project builds gets `FPAG_CXX_STDLIB` too, and always
  agrees.
- A dependency that was already installed was built with whatever standard
  library *its* build used. Pointing `FPAG_CXX_STDLIB` somewhere else is a
  mismatch that only shows up at link time, as undefined `std::__1` or
  `std::__cxx11` symbols. The configure step warns when it sees this; to resolve
  it, build the dependency here too, for example
  `-DCMAKE_DISABLE_FIND_PACKAGE_fmt=ON`.
- A consumer of an installed `libfpag` has to be compiled against the same
  standard library. Nothing records it, so it has to be communicated.

`FPAG_ENABLE_LIBUNWIND` resolves libunwind with `find_library` and links the
absolute paths, rather than passing `-lunwind`. That is deliberate: LLVM ships
a libunwind of its own as `libunwind.so.1`, and an LLVM toolchain puts its
library directory ahead of the system one, so a bare `-lunwind` picks the wrong
one and the GNU unwinder entry points end up unresolved. `find_library` does
not read `LDFLAGS`, so it cannot be confused that way.

## Conventions

- **Standard.** C++20, no exceptions, no RTTI, anywhere. The tests and the
  benchmarks included, because they are held to the same contract.
- **Types.** Use the integer aliases from `fpag/base/numeric.h` (`i32`,
  `usize`, `f64`, and so on) rather than the raw C++ spellings. They are in the
  global namespace on purpose.
- **Ownership.** fpag containers are single-owner, so copy is deleted and move
  is the default. If you find yourself wanting a copy, that is usually the
  signal that the data should be interned, pooled or referenced instead.
- **Naming.** `PascalCase` for types, `kPascalCase` for constants,
  `snake_case` for everything else. Include guards are `#pragma once`.
- **Header layout.** One module per directory, and the directory name, the
  namespace and the header name agree. A public header includes what it uses;
  a private header in `src/` includes what it uses.
- **Includes.** Root relative, so `#include "fpag/base/result.h"`, never a
  relative `../../`. Group them the way `.clang-format` wants them, which is
  what `--sort-includes` maintains for you.
- **Comments.** English, and only for design rationale, an invariant, a safety
  argument, or something genuinely non-obvious. Do not restate what the code
  already says, and do not write a comment that explains why you did not do
  something. If a `TODO` is warranted, make it `TODO(scope):` so it is
  greppable.
- **Files.** New source files need the license header, which
  `cmake --build <dir> --target license` applies. New `.cc` files also need an
  entry in the source list in `cmake/FpagTargets.cmake`; the list is explicit on
  purpose, so that adding a file shows up in review and a stale build directory
  can never silently drop a translation unit.
- **Language.** Code, comments, documentation and commit messages are in
  English.

## Tests

`tests/` is a single Catch2 binary, `fpag_tests`, with `tests/test_main.cc` as
its `main`. That main is not boilerplate: it installs the process level
handlers and wraps the Catch2 session in the profiler, which a
`Catch2::Catch2WithMain` target could not do.

```bash
ctest --preset dev                                  # everything
ctest --preset dev -R 'logging'                     # by name
ctest --preset dev -R 'CompositeSink' --output-on-failure
./build/dev/fpag_tests '[sink]'                     # Catch2 tags, directly
```

Tag a new test with the module it covers, `[logging]`, `[mem]`, `[arg]`, and
narrow it further if the name is not already specific. `catch_discover_tests`
turns each `TEST_CASE` into its own ctest entry, so a test that is a real
invariant gets a test of its own rather than an extra block in a bigger one.

When you change behavior, add a test that fails without the change. When you
fix a bug, add the test that was missing.

Benchmarks are separate, behind `FPAG_BUILD_BENCHMARKS`, and are for
measuring a change rather than for gating one:

```bash
./build/dev/fpag_benchmarks --benchmark_filter=spsc_queue
```

CI does not build them, and that is deliberate rather than an oversight:
`benchmarks/async_logger_bench.cc` crashes Clang 22.1.8 intermittently at
`-O3`, in the mangler, on the `FMT_COMPILE` uses inside the `Logger` concept.
It reproduces on code from before the CMake migration, so it is an upstream
compiler bug. See the known tensions in [ARCHITECTURE.md](ARCHITECTURE.md).
The `dev` preset builds them, at `-O0`, where it does not trigger.

## Changes and review

- Keep a change focused. Do not fold a refactor into a behavior change unless
  the two cannot be separated.
- When a change alters module boundaries, the dependency graph, a lifetime
  rule or any other design-level contract, update `ARCHITECTURE.md` in the same
  change. The document and the code must not drift apart.
- When a change makes something possible that was not, or makes something
  impossible that was, say so in the commit message. That is the information a
  reviewer cannot reconstruct from the diff.
- Make sure CI passes before asking for a review. It runs the test suite on
  Linux, macOS and Windows in both Debug and Release, a sanitizer pass, a
  coverage pass, `clang-tidy`, `clang-format` and `cpplint`, and a job that
  installs the package and compiles a consumer against it.
- The project is pre-1.0, so the API still moves. Call breaking changes out in
  the commit message rather than letting them hide in a rename.

### Commit messages

[Conventional Commits](https://www.conventionalcommits.org/), one line of
summary in the imperative, then a body that says why:

```text
fix(logging): construct the sink in init() instead of before it

Both SyncLogger and BackendWorker held their sink as a plain value member,
so it was default constructed as part of the logger ...
```

The scope is the module, in the parentheses: `logging`, `mem`, `arg`, `io`,
`build`, `ci`, `docs`.

## License

Apache License 2.0. See [LICENSE](LICENSE).
