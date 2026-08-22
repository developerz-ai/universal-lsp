# 0004 — graph-lite's embedding surface is a stable C ABI

**Status:** Accepted
**Date:** 2026-08-22

## Context

graph-lite is pitched as *the* embeddable graph library — the SQLite comparison is
not just about being in-process, it is about **who can embed it**. SQLite's reach
comes from a flat C ABI: every language runtime on earth can call C, and almost
none can call C++.

A C++-only surface limits graph-lite to C++ callers. That is a small fraction of
the projects that want in-process graph storage. Ruby wants it through FFI, Node
and Bun through N-API or `bun:ffi`, Python through `ctypes`/`cffi`, Rust through
`extern "C"`, Go through cgo — and every one of those needs the same thing: a
header with C linkage, opaque handles, integer error codes, and no C++ types
crossing the boundary.

C++ cannot provide that. It has no stable ABI across compilers or even across
standard-library versions; `std::string` in a signature makes the library
unlinkable from anything not built by the same toolchain, and unusable from any
non-C++ caller at all.

This is a **one-way door**, and it has to be decided before the API hardens: a C
ABI retrofitted onto a C++-shaped design is a second, worse API wrapping the
first, and every binding pins whichever one shipped.

## Decision

**The C ABI is graph-lite's primary public contract.** The C++ API is a
header-only convenience layer over it, not the other way around.

| Header | Language | Is |
|---|---|---|
| `graph-lite.h` | C (usable from C++) | **The contract.** `extern "C"`, opaque handles, integer error codes, caller-owned buffers. |
| `graph-lite.hpp` | C++ | Header-only ergonomics — RAII handles, spans, exceptions — implemented entirely in terms of `graph-lite.h`. Adds no exported symbol of its own. |

Rules that follow, and are not negotiable per-function:

- **No C++ type crosses the boundary.** No `std::string`, no `std::vector`, no
  templates, no exceptions escaping, no `bool` with C++ semantics assumed.
- **Opaque handles**, never a struct whose layout callers depend on. Struct
  layout in a public header is an ABI promise nobody wants to keep.
- **Integer error codes**, never a thrown exception and never a bare null that
  collapses several failure reasons into one. Every failure a binding must
  distinguish gets a distinct code.
- **Caller-owned buffers with explicit sizes**, or an allocation paired with an
  explicit `graph_lite_free_*`. Bindings cannot free a pointer that a different
  allocator produced.
- **No callbacks into the caller's runtime on a hot path.** A garbage-collected
  runtime re-entering during a traversal is how a binding deadlocks.
- **Symbol-versioned and additive.** Removing or changing a C symbol breaks every
  binding built against it, silently, at load time.

## Consequences

- Reach: any language with an FFI can embed it, which is the entire point of the
  SQLite comparison.
- The C++ caller inside this repository uses `graph-lite.hpp` and pays nothing —
  it is header-only and inlines away.
- Cost: designing in C is more work than designing in C++, and some ergonomics
  (iterators, ranges) have to be expressed as cursor handles. Accepted; that cost
  is paid once, by us, instead of by every binding author.
- **Reference bindings are a separate deliverable**, not part of the library.
  Build phase 13 — they exist to prove the ABI is usable from outside, which is
  the only honest test of a C surface designed by C++ programmers.
- A Lua binding would collide with the vocabulary guard, which bans `lua` as a
  consumer dependency. If that becomes real, the answer is to revisit the ban
  deliberately — not to add an allowlist entry.
- Error codes and handle semantics join the persisted layout and node id scheme on
  the short list of things that cannot be walked back after the first release.

## Alternatives rejected

- **C++-only surface.** Simplest to write, and it caps adoption at C++ projects.
  It also makes the library unlinkable across toolchains, which embedders hit
  immediately.
- **C++ core with a C wrapper added later.** The wrapper ends up shaped by C++
  decisions it cannot undo — string ownership, exception-based errors, layout
  assumptions — and bindings inherit all of it.
- **A serialised RPC/IPC surface (JSON, protobuf) instead of an ABI.** Defeats
  the purpose: the whole value is being in-process, with no serialisation between
  the caller and the graph.
