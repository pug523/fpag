# Architecture

This document is about *what* fpag is and *why* it is shaped this way. For how
to build, test, lint and contribute, see [CONTRIBUTING.md](CONTRIBUTING.md).

Implementation details may drift while the project is young. This document is
updated whenever a design or an architectural contract changes, and a change
that invalidates it without updating it is the change that is wrong.

## What fpag is for

fpag is the layer a C++ project puts under everything else: arenas, a logger,
a command line parser, and the platform plumbing those need in order to work
the same on Linux, macOS and Windows.

Three properties are not negotiable and shape almost every decision below.

**No exceptions, no RTTI.** The whole library, its tests and its benchmarks
compile with `-fno-exceptions -fno-rtti`. Recoverable failures are values, in
`base::Result` or in the `arg` module's own result type. Invariant violations
are crashes, on purpose, with a stack trace.

**Configuration is a template parameter, not a runtime setting.** The log
level, the sink, the wait strategy, the CLI formatter, the string to T
converter, the argument codec: each is chosen in the type system. This is why
there is not one virtual function in the public API.

**The hot path allocates nothing.** Logging a record, interning a string,
allocating from an arena: none of these reach for the general allocator in
steady state. A logging library that calls `new` on every line is not a fast
logging library.

## Module map

Thirteen modules, plus the umbrella header. `build` is the only one with no
dependencies, and `logging` sits near the top of the graph while still feeding
`debug`.

| Module | What it is for | Header only |
| --- | --- | --- |
| `build` | Platform, compiler and architecture detection macros | yes |
| `hardware` | `cpu_yield()` and other CPU primitives | yes |
| `term` | Terminal capability detection, ANSI styling, color policy | no |
| `base` | `Result`, `Vec`, `SooVec`, `TaggedUnion`, `Union`, `Idx`, integer aliases | yes |
| `hash` | XXH3 hasher | yes |
| `io` | File descriptors, memory mapped files, temporary files and directories | no |
| `mem` | Page allocation, arenas | no |
| `container` | Lock-free SPSC queue, fixed-capacity concurrent hash map | partly |
| `str` | String pool, string interner | partly |
| `debug` | Assertions, fatal crashes, stack traces, profiler, process handlers | no |
| `logging` | `SyncLogger`, `AsyncLogger`, sinks, codecs, wait strategies | yes |
| `arg` | Command line parsing, subcommands, typed conversion | no |
| `testing` | Instrumented probe types for the test suite | yes |

The `Header only` column matters more than it looks: eight of the thirteen
modules cost nothing to include. `logging` is the largest module in the
repository and has no `.cc` file at all.

## Dependency direction

An arrow means "may include". The graph is acyclic, and that is enforced by
review rather than by tooling, so it is worth stating explicitly.

```text
  build --> hardware --> base --> mem --> container --> str --> hash
    |          |          |        |                      |
    |          |          |        +----------------------+
    |          |          |                      |
    v          v          v                      |
   term -----------------------+                  |
                              |                  |
   io --------------------->  debug <--------------+
                              ^
                              |  debug/logger.h, and only that header
                              v
                           logging
                              ^
                              |
                             arg
```

Read the diagram as layers rather than as a single chain. `build` is the
floor and depends on nothing. `base`, `mem`, `io`, `term` and `hardware` are
the substrate. `container`, `str` and `hash` sit on the substrate and are
usable by anything above. `debug` and `logging` sit in the middle, mutually
aware in a specific and narrow way. `arg` sits on top and depends downward
only.

Two edges break that layering, and both are deliberate:

- **`debug` -> `logging`** exists, in `debug/logger.h`, and nowhere else. It is
  what gives assertions and crash handling somewhere to write.
- **`logging` -> `debug`** also exists, but reaches only `debug/check.h` and
  `debug/time_util.h`. Neither of those includes `debug/logger.h`, so the pair
  is not a cycle. **This is the one place where the layering is delicate.**
  Adding a `logging` include to `debug/check.h` closes the cycle immediately,
  and the compiler will not tell you, because the cycle is not inside one
  translation unit.

The rule behind all of it: **a module may depend on `debug`, and `debug` may
depend on `logging`, but `logging` may only use the two leaves of `debug` that
cannot reach back.**

- `build` depends on nothing. It is the root, and it is a vendored copy of
  Chromium's `build/build_config.h` rather than something written for fpag.
- `base` depends on `build` and `debug`. The `debug` edge is real: `Result`
  uses `FPAG_DCHECK`, and a check is a diagnostic.
- `arg` sits on top of `base`, `debug` and `term`, and reaches into no logging.
  Parsing a command line is not a logging concern.
- `io` is reachable from `debug` and from the file sinks, and from nowhere
  else.

## The three design commitments

### Compile-time configuration

A concept per extension point, and every concrete type checks itself against
the concept at its own definition rather than at the point of use.

| Concept | Decides | Checked by |
| --- | --- | --- |
| `logging::Sink` | Where a record ends up | `static_assert(Sink<X>)` in each sink |
| `logging::WaitStrategy` | How the async worker idles | each implementation |
| `logging::Logger` | The seven level methods and three call shapes | benchmarks |
| `arg::Converter<T>` | String to `T` | `static_assert(Parsable<T>)` at use |
| `arg::ErrorFormatter` / `HelpFormatter` / `VersionFormatter` | Diagnostic rendering | each implementation |
| `Codec<T>` | How an argument crosses the queue | each specialization |

`LogLevel kMinLevel` is a non-type template parameter, so the threshold test
is `consteval` and the guard is `if constexpr`. A `logger.trace(...)` in a
build whose minimum level is `Info` does not compile down to a branch. It does
not compile down to anything: the arguments are not evaluated.

A `Sink` is move constructible and is *not* required to be default
constructible or move assignable. The sink is created by `init()` and does not
exist before that, which is what lets a sink hold configured state, aggregate
other sinks, or own a resource outright. See `include/fpag/logging/sink/sink.h`.

### No allocation on the hot path

- Formatting goes into `format_buffer`, a 4 KiB `fmt::basic_memory_buffer`
  on the stack.
- The async path encodes each record into the SPSC queue's ring, which is one
  contiguous `mmap` reservation. Nothing is allocated per record.
- A `FMT_COMPILE`d format string is a template parameter, so it is not stored
  on the wire at all. An interned one crosses as an 8-byte id.
- `FileSink` and `JsonLinesSink` format straight into the mapped region via
  `prepare_write_buffer` / `commit_write`, so a log line costs no syscall and
  no allocation.
- Arenas reserve address space up front and commit pages as they are handed
  out. A 1 GiB `StringPool` costs nothing until it is filled.

The single exception is `Serializer`'s slowest branch, which stages variable
length arguments in a 4 KiB stack buffer before copying them into the ring.

### Lock-free where it is on a hot path

- `container::SpscQueue` separates the producer's and the consumer's counters
  onto their own cache lines and gives each side a private, non-atomic mirror
  of the other's. The steady-state loop touches no shared atomic at all.
- `container::SimpleConcurrentHashMap` locks a slot with a compare-exchange
  on its hash, and never resizes. Insert contention shows up as a failed
  `try_insert`, not as a blocked thread.
- `mem::ConcurrentArena` is a compare-exchange bump pointer, with a second
  compare-exchange loop to commit the pages the new allocation reached.
- The default back-pressure policy on the log queue is to **drop**, not to
  block. A full queue loses records and increments `dropped_count()`. Blocking
  is available, and escalates through a four stage backoff, but a logger that
  can stall the application is not usable inside one.

## Logging

The pipeline, end to end:

```
producer thread                          │  consumer thread
─────────────────────────────────────────┼──────────────────────────────────
AsyncLogger::info(fmt, args...)          │
  if constexpr (level < kMinLevel) skip  │   ← gone at compile time
  Serializer::serialize_to               │
    SpscQueue::reserve / commit          │
      [size][DeserializeFunction][level] │
      [interned id | nothing]            │   BackendWorker::worker_loop
      [encoded arguments]                │     process_batch()
  ─── no allocation, no syscall ────     │       deserialize
                                         │       format_buffer
                                         │       sink.log(entry)
```

`BackendWorker` runs a five state machine, `NotInitialized` → `Initialized` →
`Running` → `Stopping` / `ForceStopping`, and every transition is checked. The
sink is engaged exactly while the worker is `Initialized` or `Running`, which
is what makes a sink that has never been handed a `log()` call impossible
rather than merely unlikely.

Three deliberate choices here:

- **The worker is a consumer of a byte ring, not a holder of `LogEntry`.** The
  type-erased deserializer function pointer travels in the payload, which is
  what lets one ring hold records with different argument types.
- **Batching is the throughput story, not queueing.** `process_batch()` drains
  the whole queue and takes one timestamp for the batch. `SyncLogger` takes one
  per record.
- **The async producer never touches the sink.** That is why the sink
  initialization fix costs the async path nothing at all.

Two known rough edges, both documented in the code: `Serializer` only
`DCHECK`s the total size of its 4 KiB staging buffer, and neither file sink
carries a source location because `LogEntry` has no field for one yet.

## Memory and lifetime

`mem::Arena` is a bump allocator with no `free`. `ArenaDeleter` calls the
destructor and does not release the bytes, and `ArenaUniquePtr<T>` wraps that
into a `unique_ptr`, so a mistake becomes a compile error rather than a double
free. `Arena` and `ConcurrentArena` share the API; the difference is that
`ConcurrentArena::alloc` is a compare-exchange loop and is safe from several
threads, while `reserve` and `reset` on either are not and must happen before
the arena is shared.

`str::StringPoolId` is a fixed 8 bytes on every platform, deliberately, so
that anything embedding it stays architecture independent. Interning never
frees: a thread that loses the `try_insert` race has its pool bytes stranded,
and that is the price of every outstanding `string_view` staying valid.

## Error model

Three levels, and nothing falls between them.

1. **A value.** `base::Result<T, E>` for a recoverable failure, with
   `map` and `and_then` for chaining, and no exceptions. `Result` is move-only
   and gives you `const&` and `&&` overloads so a payload can be moved out only
   when the caller is done with it.
2. **A diagnostic.** `FPAG_CHECK` and its relatives report, then abort. They
   write through the debug logger and print a stack trace, so a failure in a
   debug build is actually diagnosable.
3. **A trap.** `FPAG_UNREACHABLE` compiles to `__builtin_unreachable` in
   release. Reaching it is undefined behaviour by design, and the log line
   before it exists to tell you how you got there.

`FPAG_RAW_CHECK` is the variant that promises no heap allocation on the
failure path, for use where the heap, or the logger itself, is the thing that
is broken.

## Process level wiring

Nothing in fpag runs from a static constructor. Every hook is an explicit,
opt-in call, and a library that installs signal handlers behind a consumer's
back is a library that makes the consumer's own handlers impossible to test.

```cpp
term::register_console();               // detect the terminal, enable VT on Windows
debug::init_debug_logger();             // give the debug logger its sink
debug::register_exit_handler();         // reset ANSI color on exit
debug::register_terminate_handler();    // std::terminate -> log + stack trace + trap
debug::register_signal_handlers();      // SIGSEGV and friends -> the same funnel
debug::Profiler::global().start();      // if profiling
```

The crash paths all converge on `debug::internal::fatal_crash_impl()`, and each
one **flushes the logger before trapping**, because a buffered sink would
otherwise lose the very message that explains the crash. `DebugLogger` is a
synchronous, unbuffered `SyncLogger` for exactly this reason: an asynchronous
logger is worthless in a process that is about to die.

## Ports and build contract

`build/build_config.h` is a vendored copy of Chromium's, with attribution. It
detects the platform, the compiler and the architecture, and emits roughly
seventy `FPAG_BUILD_FLAG_INTERNAL_*()` macros that `FPAG_BUILD_FLAG(name)` turns
into a `constexpr` `0` or `1`. The point of the extra indirection is that the
result works in `#if`, in `if constexpr` and in an array bound, so platform
behavior can be a template parameter rather than a duplicated code path.

Everything else the build owes the code is in
[CONTRIBUTING.md](CONTRIBUTING.md). In short: C++20, no exceptions, no RTTI,
warnings as errors, and a public interface that is exactly
`include/fpag/**` plus `fmt` and `xxhash`.

## Known tensions

Recorded rather than hidden, so that a future change does not rediscover them
as if they were new.

- `include/fpag/base/logger.h` declares `base::Logger` and `base::init_logger`.
  Nothing defines them and nothing includes the header, so it would fail to
  link. It is also the only `base` → `logging` edge in the graph. It should be
  deleted, or defined; it should not be left where it is.
- `fpag.h` is a convenience, not a specification. It omits `arg`, `testing`,
  `hardware`, the `Logger` concept and every sink. Including it does not give
  you the library.
- `LogEntry` has no source location, so the two file sinks carry a `TODO` and
  cannot annotate a line. `debug::Location` exists and is unused here.
- The DWARF inlined-line path in `stack_trace/symbolicator.h` is commented out,
  which is why the file references a `debug/dwarf/provider.h` that does not
  exist. Symbolication resolves functions, not lines within them.
- The project is young, and the public API still changes. `base::numeric.h`
  exporting into the global namespace is the kind of decision that is much
  cheaper to make now than in a year.
- `benchmarks/async_logger_bench.cc` crashes Clang 22.1.8 intermittently at
  `-O3`, inside the Itanium mangler, while mangling the `FMT_COMPILE` uses
  inside the `Logger` concept. It reproduces on code from before the CMake
  migration, roughly one run in three, so it is a compiler bug rather than
  something here. It is the reason CI does not set `FPAG_BUILD_BENCHMARKS`: a
  coin flip in CI is worse than not compiling the benchmarks there. The
  benchmarks are still built and run by the `dev` preset, where `-O0` does not
  trigger it.
