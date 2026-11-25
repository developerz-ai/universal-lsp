# graph-lite standalone isolation

## Available failure evidence

Two documented failures bound what is known about graph-lite's isolation, and neither is currently reproducible from this checkout:

- **PR #30** — closed, never merged. The change it proposed (whatever its content was) did not land, so the tree has no record of what it touched or why it failed.
- **run_652a1efa635246ce9825262689a76e09** — not a GitHub Actions run id, so its CI log is unavailable through `actions/` or `gh`. Whatever run it named left no accessible log here.

Both absences are themselves the evidence: nothing in this repo records a failed graph-lite standalone build, because the `standalone` CI job has stayed green. The guard that keeps it that way is the job itself plus `scripts/lint/graph-lite-boundary.ts`.

## What graph-lite depends on outside `graph-lite/`

### Build (library)

When `graph-lite/` is configured standalone (`cmake -S graph-lite`), it depends on nothing outside its own directory: `graph-lite/CMakeLists.txt` defines the `ulsp_warnings` and `ulsp_sanitizers` INTERFACE targets inline when `CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR`.

When embedded via `add_subdirectory(graph-lite)` from the root build, graph-lite's `target_link_libraries(graph-lite PRIVATE ulsp_warnings ulsp_sanitizers)` resolves targets defined by the parent:

- root `CMakeLists.txt` — calls `add_subdirectory(graph-lite)`, `include(CompilerWarnings)`, `include(Sanitizers)`, and sets `ULSP_WERROR`/`ULSP_SANITIZE`
- `cmake/CompilerWarnings.cmake` — defines `ulsp_warnings`
- `cmake/Sanitizers.cmake` — defines `ulsp_sanitizers`

### Tests

`tests/graph-lite/CMakeLists.txt` registers suites with `ulsp_add_test`, which is defined in:

- `tests/CMakeLists.txt` — the `ulsp_add_test` helper (also links `GTest::gtest_main`)
- `cmake/Dependencies.cmake` — `FetchContent` of GoogleTest from `https://github.com/google/googletest.git` (tag v1.17.0), unless a system GTest is found; this is the one network dependency, test-only
- root `CMakeLists.txt` — `ULSP_BUILD_TESTS`, `include(Dependencies)`, `add_subdirectory(tests)`

Test suites: `graph_lite_version_test`, `graph_lite_store_test`, `graph_lite_alloc_test`, `graph_lite_model_test`, `graph_lite_model_properties_test`, `graph_lite_traversal_test`. The C ABI is additionally exercised by `tests/graph-lite/c_abi_test.c`, compiled as C.

### CI

- `.github/workflows/ci.yml` — `standalone` job runs `cmake -S graph-lite -B build/graph-lite-standalone -G Ninja -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build/graph-lite-standalone`; the aggregate `ci` job needs `standalone`, so a red standalone job fails CI
- `.github/workflows/release.yml` — also builds graph-lite standalone (Release) as part of the release flow

### Embedding / README

- `docs/graph-lite/README.md` — records `cmake -S graph-lite -B build/graph-lite -G Ninja` and `add_subdirectory(graph-lite)` as the two supported ways to consume it; claims both need nothing else from the repository
- root `CMakeLists.txt` — `add_subdirectory(graph-lite)` for embedding

### Doc cross-references (read-only, not build dependencies)

- `graph-lite/README.md` → `../docs/architecture/decisions/0004-graph-lite-c-abi.md` (ABI rationale), `../bindings/` (reference Ruby/Bun/Python ABI probes, not supported packages)
- `graph-lite/CLAUDE.md` → `../docs/architecture/graph-lite-boundary.md` (boundary enforcement reasoning)
- `docs/graph-lite/persistence.md` — persistence format, referenced from `graph-lite/CLAUDE.md`

## What the isolation proof is

`cmake -S graph-lite -B build/graph-lite-standalone -G Ninja && cmake --build build/graph-lite-standalone` configures and builds a clean copy of only `graph-lite/` with no dependency outside `graph-lite/` and no network. `ctest` on that build runs the suites registered through `ulsp_add_test`; any failure means graph-lite has grown a dependency it must not have, or a test needs borrowed vocabulary, and both are caught by `scripts/lint/graph-lite-vocabulary.ts` and `scripts/lint/graph-lite-boundary.ts` respectively.
