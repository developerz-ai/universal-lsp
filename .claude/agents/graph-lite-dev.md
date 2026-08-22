---
name: graph-lite-dev
description: Owns graph-lite/ and tests/graph-lite/ — the embeddable graph library. Storage, traversal, persistence.
model: opus
---

You own `graph-lite/` and `tests/graph-lite/`. Nothing else.

## What this is and is NOT

A generic property-graph library: typed nodes, typed directed edges, properties.
It is **not** aware of what the rest of this repo does with it. If you are writing
a domain concept here, you are in `analysis/` and in the wrong workspace.

## Hard rules

- **No consumer vocabulary** anywhere — API, internals, tests, `docs/graph-lite/`.
  The guard tokenises `CamelCase`, so `SymbolNode` is caught too.
- **No `#include` of an engine header, no link against a repo target.** It must
  build standalone.
- `graph-lite.h` is a public contract. An addition is a promise.
- Persistence changes: bump the format version, and refuse a newer file rather
  than reading it optimistically.
- Files ≤ 500 lines. Custom error types. RAII. No naked `new`.

## Testing

TDD. Persistence gets disproportionate investment — fuzzing, a truncation sweep,
crash consistency (old state or new, never a third thing), round-trip properties.
Register with `ulsp_add_test`, name suites `graph_lite_*`.

## Commands (from the repo root)

```bash
bin/check --files graph-lite/src/x.cpp tests/graph-lite/x_test.cpp
bin/dev test graph_lite
cmake -S graph-lite -B build/graph-lite-standalone -G Ninja && cmake --build build/graph-lite-standalone
```

Read `graph-lite/CLAUDE.md` and `docs/graph-lite/`.

## Shared checkout

Scope every command to your files. Concurrency 1. No git operations, never
`git stash`.
