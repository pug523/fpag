# AGENTS.md

Instructions for an AI agent working in this repository. Written to be read by
an agent, not by a person; the human-facing version of everything below is
[CONTRIBUTING.md](CONTRIBUTING.md) and [ARCHITECTURE.md](ARCHITECTURE.md).

## Reference documentation

Read these before changing anything. Do not guess at the design.

- `ARCHITECTURE.md`: what each module is for, the dependency graph, the three
  design commitments, the logging pipeline, the error model, and the known
  tensions. This is the one that matters.
- `CONTRIBUTING.md`: build commands, presets, options, conventions, the test
  layout, commit format.
- `README.md`: module list and the two ways to consume the library.
- `include/fpag/README.md`: one line per module.

## The contract you must not break

- **C++20, `-fno-exceptions`, `-fno-rtti`.** Not in the library, not in the
  tests, not in the benchmarks. Recoverable failure is `base::Result<T, E>` or
  the `arg` module's own result type. An invariant violation is
  `FPAG_CHECK` or `FPAG_UNREACHABLE`.
- **Configuration is a template parameter.** Log level, sink, wait strategy,
  CLI formatter, string converter, argument codec. Do not introduce a virtual
  function, a runtime strategy pointer or a settable property to make something
  configurable. If a caller needs a different configuration, they instantiate a
  different type.
- **The public interface is `include/fpag/**`, plus `fmt` and `xxhash`.** Adding
  a third-party header to a public header makes it a public dependency of every
  consumer, so it needs a real justification.
- **No allocation on a hot path.** Logging a record, interning a string,
  allocating from an arena. Use the existing stack buffers, arenas and
  interners. A `new` in a logging call is a defect.
- **The module dependency graph is acyclic.** A module may depend on `debug`,
  and `debug` may depend on `logging`, but `logging` may only use `debug/check.h`
  and `debug/time_util.h`. Anything else closes the cycle, and the compiler will
  not tell you, because the cycle is not within one translation unit.
- **Walls are hard.** `build/` is the root and depends on nothing.
  `fpag/build/build_config.h` is a vendored copy of Chromium's, with
  attribution; do not edit it to add a platform, extend it in `build_flag.h`
  terms instead.

## Working rules

- **Read before writing.** Match the surrounding code: its naming, its comment
  density, its idiom. This codebase is `snake_case` functions,
  `PascalCase` types, `kPascalCase` constants, `FPAG_SNAKE_CASE` macros, and
  the integer aliases from `fpag/base/numeric.h` rather than raw `int`/`size_t`.
- **Match the build system, do not work around it.** New source files go into
  the explicit list in `cmake/FpagTargets.cmake`; a new build knob goes into
  `cmake/FpagOptions.cmake`; a new flag goes into
  `_fpag_add_probeable_options`. Never set a global compiler flag, never
  `include_directories()`, never `add_definitions()`: this tree is consumed
  through `add_subdirectory()`, and a global setting leaks into the parent
  project.
- **Fix a platform problem in the build, not per source file.** The
  `_CRT_SECURE_NO_WARNINGS` and libunwind problems were both fixed at the build
  level on purpose. A `#define` inside a `.cc` that fixes a missing compiler
  flag is in the wrong place, and is silently ineffective if the file includes a
  header before the define.
- **Build with the project's toolchain.** Clang, and `cmake --preset dev`. GCC
  is stricter about `-Wconversion` under `-Werror`, so a green GCC build is not
  proof and a red one may not be your fault. Check the flags CI uses before
  concluding that a pre-existing warning is yours.
- **Run the tests before claiming anything works.**
  `ctest --preset dev`, and `ctest --preset asan` when you touched memory,
  lifetimes, arenas or anything lock-free. All 126 cases must pass.
- **A log call before `init()` is a defect, not a style question.** The sink is
  held in a `std::optional` for exactly this reason. Do not reintroduce a
  default-constructed sink, and do not add a default constructor to a sink to
  make a problem go away.
- **Prefer a constraint to a comment.** When you find yourself documenting an
  invariant on a template, encode it: a concept, a `static_assert`, a deleted
  overload. `Sink` requires `std::move_constructible` and deliberately does not
  require default constructibility or move assignibility; keep it that way.
- **Prefer `ast-grep` (`sg`) for repetitive, structural edits.** Manual
  multi-file substitutions get one of the sites wrong and the tests will not
  always notice.

## Language and output rules

- **All code, comments, documentation and commit messages are in English.**
- **Do not write comments that reference the conversation.** No "the user asked
  for", no "as discussed", no "previously", no "note that we could also have".
  A comment describes the code, forever, with no knowledge of why it was
  written.
- **Do not write multi-line justifications for ordinary choices.** One line for
  the invariant, one line for the safety argument, and stop. If a choice needs a
  paragraph, it belongs in `ARCHITECTURE.md`.
- **Do not add a comment restating the code.** `++i;  // increment i` is worse
  than nothing.
- **Do not add a file, a class, a function, an option or a dependency to
  "prepare for" or "make room for" a hypothetical.** The project is pre-1.0 and
  the API still moves; speculative scaffolding is pure cost.
- **Do not silently widen scope.** A formatting fix, a rename and a behavior
  change are three commits. If you find an unrelated problem, report it, and do
  not fold it into the change you were asked to make.
- **Report honestly.** If a test fails, say which and why. If a change is a
  workaround rather than a fix, say that. If you could not verify something,
  say that too, instead of implying you did.

## Before you commit

```bash
ctest --preset dev
cmake --build build/dev --target lint
cmake --build build/tidy --target tidy
```

`lint` is `clang-format --dry-run --Werror` plus `cpplint`; `tidy` is
`clang-tidy --fix` plus `clang-format -i`. Both can modify files, so run them
before staging, and check `git status` afterwards.

New source files need the license header, which
`cmake --build build/dev --target license` applies.

Commit with [Conventional Commits](https://www.conventionalcommits.org/) and
a body that says *why*, not *what* the diff already shows. The scope is the
module: `logging`, `mem`, `arg`, `io`, `build`, `ci`, `docs`.

## Repository layout

```text
include/fpag/<module>/   public headers, one directory per module
include/fpag/fpag.h      umbrella header, a convenience not a specification
src/<module>/            .cc files, mirroring the module directories
tests/                   one Catch2 binary; test_main.cc owns main()
benchmarks/              one Google Benchmark binary
cmake/                   one Fpag*.cmake per concern, plus two helper scripts
third_party/             vendored code, clang-format-ignored
```

Adding a module means a new directory in both `include/fpag/` and `src/`, an
entry in each source list, a line in both READMEs, and a decision about where it
sits in the dependency graph.
