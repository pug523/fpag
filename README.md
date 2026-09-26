# fpag

A fast, light-weight utility library for C++20

> [!NOTE]
> This project is now work in progress.

Arenas, a lock-free logger, a command line parser, and the platform plumbing
underneath them. The library is a single static `libfpag`, has no exceptions
and no RTTI, and exposes no virtual functions.

## Modules

`arg`, `base`, `build`, `container`, `debug`, `hardware`, `hash`, `io`,
`logging`, `mem`, `str`, `term`, `testing`, and the `fpag.h` umbrella header.
[ARCHITECTURE.md](ARCHITECTURE.md) covers what each one is for and how they
are allowed to depend on each other.

## Build

Everything that is not already installed is fetched automatically, so a bare
checkout configures with nothing but a compiler and CMake 3.28 or newer.

```bash
cmake --preset default --build
ctest --preset default
```

The presets are `default`, `debug`, `release`, `dev` (tests and benchmarks),
`asan`, `coverage`, `tidy` and `native`. Run `cmake --list-presets` to see
them all.

To consume it from another CMake project, either add this tree with
`add_subdirectory()` and link `fpag::fpag`, or install and use
`find_package`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install
cmake --build build && cmake --install build
```

```cmake
find_package(fpag 0.1 REQUIRED)
target_link_libraries(app PRIVATE fpag::fpag)
```

`libfpag` is static, so a consumer has to be compiled against the same
standard library the library was built with. `FPAG_CXX_STDLIB` records that
choice for the build, and it is not recorded anywhere in the installed
package.

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md): what the library is and how it is put
  together.
- [CONTRIBUTING.md](CONTRIBUTING.md): day to day build, test, lint and review
  rules. Read this one before opening a pull request.

## License

Apache License 2.0. See [LICENSE](LICENSE).
