# graph-lite

A small, fast, dependency-free, embeddable property-graph library for C++20.
Typed nodes, typed directed edges, arbitrary properties on both. One public
header, one static library, no server, no query language, no runtime
dependencies.

> **v0.0.1 — pre-implementation.** The header exposes version information only.
> The model, storage, traversal and persistence are designed and being built.

## Embed it

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Or vendor the directory and `add_subdirectory(graph-lite)` — it configures both
ways, and the standalone path is verified on every change.

```cpp
#include "graph-lite.h"

const auto v = graph_lite::version();
// compare against GRAPH_LITE_VERSION_* to catch a header/binary mismatch at
// startup rather than as a corrupt read much later
```

## The model

- **Node** — a stable id, a type tag, properties.
- **Edge** — directed, from one id to another, a type tag, properties.
- **Type** — an opaque tag *you* define and name. The library attaches no meaning
  to any of them, holds no registry, and reserves no name.

Nothing about your domain appears in this library. That is the design, and it is
enforced by a lint rule rather than left to review.

## What it does

Add and remove nodes and edges. Walk breadth-first or depth-first, bounded or
unbounded. Walk **reverse edges** — the "what points at this" direction — as a
first-class operation. Find the shortest or any path between two ids. Filter by
node type and edge type *during* the walk, not after it.

Storage is CSR-style: flat contiguous arrays, cache-friendly to traverse, and
close enough to the persisted layout that a reload is nearly a read rather than a
parse-and-rebuild.

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
