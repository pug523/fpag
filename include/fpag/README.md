# Public headers

Each directory is one module. `ARCHITECTURE.md` explains what a module is
allowed to depend on, and what each one is for.

* `arg/`: Command line parsing, with subcommands and typed conversion
* `base/`: Foundational types, `Result`, vectors, tagged unions, integer types
* `build/`: Platform, compiler and architecture detection macros
* `container/`: Lock-free containers, currently an SPSC queue and a
  fixed-capacity concurrent hash map
* `debug/`: Assertions, fatal crashes, stack traces, the profiler, and the
  opt-in process level handlers
* `hardware/`: CPU primitives
* `hash/`: Hashing
* `io/`: File descriptors, memory mapped files, temporary files and directories
* `logging/`: The structured logger, `SyncLogger` and `AsyncLogger`, with its
  sinks, codecs and wait strategies
* `mem/`: Page allocation and arenas
* `str/`: A string pool and a string interner
* `term/`: Terminal capability detection and ANSI styling
* `testing/`: Instrumented probe types, for asserting that a container really
  did copy, move or destroy what it should have
* `fpag.h`: Umbrella header over the frequently used ones. It is not
  exhaustive; include the specific header you need.
