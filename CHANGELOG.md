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

- `0.0.1` — public surface established as a **C ABI** (`graph-lite.h`): C linkage,
  opaque handles, integer status codes. `graph-lite.hpp` is header-only C++
  ergonomics over it and exports nothing of its own. A C translation unit in the
  suite is compiled as C and linked, so the header cannot stop being C-compatible
  silently. Version and status-code entry points only; the node/edge model,
  storage, traversal and persistence land in build phase 1.
  ([ADR 0004](docs/architecture/decisions/0004-graph-lite-c-abi.md))

[Unreleased]: https://github.com/developerz-ai/universal-lsp/commits/main
