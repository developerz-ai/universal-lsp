# 01 — Build system

> Part of [overview.md](overview.md). Depends on: none

## Files

- `CMakeLists.txt` — root; options, module path, subdirectories.
- `cmake/CompilerWarnings.cmake` — one `ulsp_warnings` INTERFACE target.
- `cmake/Sanitizers.cmake` — `ulsp_sanitizers`, driven by `ULSP_SANITIZE`.
- `cmake/Dependencies.cmake` — GoogleTest: system first, pinned fetch otherwise.
- `CMakePresets.json` — `dev`, `asan`, `release`.
- `graph-lite/CMakePresets.json` — standalone `default` preset, ships on extraction.
- `<subsystem>/CMakeLists.txt` — one per subsystem.
- `tests/CMakeLists.txt` — the `ulsp_add_test` helper.

## Steps

1. Root guards against in-source builds and exports `compile_commands.json`.
2. Warnings live in ONE interface target — a subsystem with its own warning set is
   a subsystem that stops being checked. `-Wconversion` and `-Wsign-conversion`
   are load-bearing: every index here is integer maths on untrusted input.
3. `graph-lite/CMakeLists.txt` configures standalone as well as as a subdirectory;
   when standalone it defines its own empty warning/sanitizer interface targets.
4. `ulsp_add_test` registers via `gtest_discover_tests`. Name suites
   `<subsystem>_*` — `scripts/lib/affected.ts` maps subsystem → CTest regex by prefix.

## Done when

- `cmake --preset dev|asan|release` all configure.
- `cmake -S graph-lite -B build/x -G Ninja` configures and builds alone.
