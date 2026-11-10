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

- `0.1.0` — the in-memory node/edge model, storage and traversal shipped as C
  entry points: store create/destroy (destroy NULL-safe), typed node/edge add
  and remove (removing a node drops its incident edges), counts, and per-id
  type/endpoint accessors; snapshot cursors — the walk runs eagerly at open, so
  later mutation cannot change a live cursor — over breadth-first by level or
  depth-first preorder, either direction, edge/node type filters applied inside
  the walk, depth and result bounds (0 = unbounded) with a truncation flag, and
  depth/arrived-via accessors; and shortest path — breadth-first over outgoing
  edges, the reported length counting nodes, required size returned before any
  buffer is touched. Binding guarantees hold in kind: opaque handles,
  fixed-width ids assigned from 1 and never reused (0 reserved), caller-owned
  buffers, no callbacks into the caller's runtime. `graph-lite.hpp` gains
  move-only RAII `Store`/`Cursor` over the C calls, still exporting nothing of
  its own. Two status codes appended, numbers being ABI — 0–7 keep their names
  and values: `GRAPH_LITE_DONE` = 8 (cursor exhausted; not an error) and
  `GRAPH_LITE_ERR_UNREACHABLE` = 9 (both endpoints exist, nothing connects
  them). Node/edge properties and persistence remain designed, not shipped.
  ([ADR 0004](docs/architecture/decisions/0004-graph-lite-c-abi.md))
- `0.0.1` — public surface established as a **C ABI** (`graph-lite.h`): C linkage,
  opaque handles, integer status codes. `graph-lite.hpp` is header-only C++
  ergonomics over it and exports nothing of its own. A C translation unit in the
  suite is compiled as C and linked, so the header cannot stop being C-compatible
  silently. Version and status-code entry points only; the model, storage and
  traversal arrive in `0.1.0` — properties and persistence remain pending.
  ([ADR 0004](docs/architecture/decisions/0004-graph-lite-c-abi.md))

[Unreleased]: https://github.com/developerz-ai/universal-lsp/commits/main
