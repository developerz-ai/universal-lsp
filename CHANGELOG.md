# Changelog

Notable changes to the **engine**. graph-lite is versioned independently — its
changes are listed under their own heading, because an embedder tracking the
library should not have to read engine entries to find them.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Repository foundation: CMake + presets (`dev`, `asan`, `release`), GoogleTest +
  CTest wiring, a three-tier gate (`bin/check --files` / `--changed` / full),
  and CI with a matrix build, a standalone graph-lite build, and a sanitizer job.
- Mechanical guards for the one-way dependency rule
  (`graph-lite-vocabulary`, `graph-lite-boundary`), plus `max-file-loc` and
  `doc-links` — each with its own test.
- The documentation set: [spec](docs/spec.md), [research findings](docs/research/things-to-know.md),
  per-subsystem design docs, three ADRs, and the standards.

### graph-lite

- `0.1.0` — first release. The public surface is a **C ABI** (`graph-lite.h`): C
  linkage, opaque handles, integer status codes, and the in-memory node/edge
  model — add/remove node and edge with cascading removal, node/edge tags,
  out/in edge-id listing, node/edge counts, and typed properties (int64,
  double, bool, string) — plus the version and status-string entry points.
  `graph-lite.hpp` is header-only C++ ergonomics over it (`Version`,
  `version()`/`version_string()`, `Error`, `check()`) and exports nothing of
  its own. A C translation unit in the suite is compiled as C and linked, so
  the header cannot stop being C-compatible silently. Traversal, CSR storage
  and persistence remain design.
  ([ADR 0004](docs/architecture/decisions/0004-graph-lite-c-abi.md))
- `0.1.0` audit: exported surface reviewed — 25 C entry points plus the hpp's
  `Version`/`version()`/`version_string()`/`Error`/`check()`; `ERR_IO`,
  `ERR_CORRUPT` and `ERR_VERSION_MISMATCH` are deliberately reserved for
  persistence (status numbers are ABI — append-only, never repurposed).
  Findings corrected: the README promised unshipped features; the hpp claimed
  an RAII handle type that does not exist; vendoring graph-lite/ into a parent
  project that defines no `ulsp_*` targets failed at configure (guarded
  INTERFACE-target fallbacks added); a header comment described the library as
  "the engine that consumes it" instead of on its own terms. Verified:
  standalone `cmake -S graph-lite` configure+build; CI builds of a clean copy
  of graph-lite/ and of a minimal vendoring parent; the `graph-lite-version`
  guard keeping the header macros and the CMakeLists version in lockstep;
  `bun test scripts/` green.

[Unreleased]: https://github.com/developerz-ai/universal-lsp/commits/main
