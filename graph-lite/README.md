# graph-lite

A small, fast, dependency-free, embeddable property-graph library. Typed nodes,
typed directed edges, arbitrary properties on both. One public header, one shared
library, no server, no query language, no runtime dependencies.

**The public surface is a C ABI**, so anything with an FFI can embed it — C, C++,
Ruby, JavaScript, Python, Rust, Go. C++ callers get a header-only ergonomics layer
on top; nobody else has to care that the implementation is C++.

> **v0.0.1 — work in progress.** The node/edge model ships behind the C ABI as an
> in-memory store: contiguous forward and reverse adjacency, stable ids that are
> never reused, tags and typed properties. Native BFS/DFS traversal ships with
> forward/reverse directions, tag filters, limits, and partial-result status.
> Direction-aware filtered any-path and unweighted shortest-path queries also
> ship through the C ABI. Persistence remains design.

## Two headers, one contract

| Header | For | Is |
|---|---|---|
| `graph-lite.h` | C, and every FFI | **The contract.** C linkage, opaque handles, integer status codes, caller-owned buffers. |
| `graph-lite.hpp` | C++ | Header-only ergonomics over the C header — RAII, real types, exceptions. Exports no entry point of its own. |

The direction is deliberate: C++ has no stable ABI across compilers or standard
library versions, so a C++ signature in a public header is unlinkable from another
toolchain and unusable from any non-C++ runtime. Rationale and the rules that
follow from it: [ADR 0004](../docs/architecture/decisions/0004-graph-lite-c-abi.md).

## Build it

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Or vendor the directory and `add_subdirectory(graph-lite)` — it configures both
ways, and the standalone path is verified on every change.

The resulting shared artifact is the C ABI for foreign runtimes. Runnable Ruby,
Bun, and Python ABI probes live in [`../bindings/`](../bindings/); they are
reference examples, not supported packages or wrapper libraries.

## Call it

```c
/* C — and the shape every binding sees */
#include "graph-lite.h"

int major, minor, patch;
graph_lite_version(&major, &minor, &patch);
/* compare against GRAPH_LITE_VERSION_* to catch a header/binary mismatch at
   startup rather than as a corrupt read much later */
```

```c
/* C — build a tiny graph. Ids are 64-bit, 0 is never valid, and an id is never
   reused after removal. */
graph_lite_graph* g = NULL;
graph_lite_id a, b, e;

graph_lite_graph_create(&g);
graph_lite_add_node(g, /* tag */ 1, &a);
graph_lite_add_node(g, /* tag */ 2, &b);
graph_lite_add_edge(g, a, b, /* tag */ 7, &e);
graph_lite_set_int(g, GRAPH_LITE_EDGE, e, "weight", 3);
graph_lite_remove_node(g, b);   /* removes the edge too */
graph_lite_graph_destroy(g);
```

```cpp
// C++ — same library, nicer surface, nothing extra exported
#include "graph-lite.hpp"

const auto v = graph_lite::version();
graph_lite::check(some_call());   // throws graph_lite::Error, which carries the code
```

Every one of those binds the same entry points. That is the point of the C ABI:
no per-language port, no serialisation, no wrapper library to keep in sync.

## Errors

Every failure a caller must tell apart has its own `graph_lite_status` code. No
exceptions cross the boundary, and no null return collapses several reasons into
one — a binding cannot recover a distinction the ABI never made.
`graph_lite_status_string()` is never NULL, including for a code from a newer
version of the header than the linked library knows.

## The model

- **Node** — a stable 64-bit id, a type tag, properties.
- **Edge** — directed, from one id to another, a type tag, properties. Self-loops
  and parallel edges are allowed; removing a node removes its incident edges.
- **Properties** — int64, double, bool or string values, stored apart from the
  adjacency lists.
- **Type** — an opaque tag *you* define and name. The library attaches no meaning
  to any of them, holds no registry, and reserves no name.

Nothing about your domain appears in this library. That is the design, and it is
enforced by a lint rule rather than left to review.

## What it does

Add and remove nodes and edges. Walk breadth-first or depth-first, forward or
reverse, with bounded or `UINT64_MAX`-unbounded depth and results. Filter node and
edge tags *during* the walk, before candidates enter the worklist; inspect explicit
partial-result status when a limit stopped an otherwise admissible candidate.

Find any admissible path or an unweighted shortest path in either direction with
the same tag filters. Paths use caller-owned node-id buffers; `NO_PATH` distinguishes
unreachable live endpoints from a successful one-node path.

Storage is dense records with per-node contiguous runs in flat forward and reverse
pools, cache-friendly to traverse without making persistence layout a promise.

## What it deliberately is not

| Not | Why |
|---|---|
| A query language | A programmatic API first. Which queries matter is what real usage answers; designing a language up front commits to guesses. |
| Backed by a relational engine | Recursive-CTE traversal cannot keep visited-set state across iterations — benchmarked 100×+ slower on deep or dense graphs. |
| Distributed, clustered, or server-mode | Embedded and in-process, permanently. |
| Schema-enforcing | Enforce at your own seam if you want it; every writer should not pay for a check most callers do not want. |

## Correctness

Silent corruption is the worst thing an embedded store can do, so the persistence
layer gets disproportionate testing: reader fuzzing, a truncation sweep over every
offset, crash-consistency (interrupt a write anywhere — the file is the old state
or the new one, never a third thing), and round-trip property tests.

## License

MIT — see [LICENSE](LICENSE). Versioned independently of anything it happens to
be built alongside.
