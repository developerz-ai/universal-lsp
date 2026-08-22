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

- `0.0.1` — public header with version information only. The node/edge model,
  storage, traversal and persistence land in build phase 1.

[Unreleased]: https://github.com/developerz-ai/universal-lsp/commits/main
