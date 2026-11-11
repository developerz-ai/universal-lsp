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
toolchain and unusable from any non-C++ runtime.

## Build and test

From this directory, with CMake 3.24+ and Ninja installed:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The CTest executable is compiled as C and exercises graph creation, mutation,
property access, and destruction through the public ABI.

## Vendor it

Copy this directory into your source tree, then add it from the consumer's top-level
`CMakeLists.txt`:

```cmake
add_subdirectory(third_party/graph-lite)
target_link_libraries(my_program PRIVATE graph-lite::graph-lite)
```

The shared `graph-lite` artifact exposes the C ABI for foreign runtimes.

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
graph_lite_graph* graph = NULL;
graph_lite_id first, second, edge;

if (graph_lite_graph_create(&graph) != GRAPH_LITE_OK)
    return 1;
if (graph_lite_add_node(graph, 1, &first) != GRAPH_LITE_OK ||
    graph_lite_add_node(graph, 2, &second) != GRAPH_LITE_OK ||
    graph_lite_add_edge(graph, first, second, 7, &edge) != GRAPH_LITE_OK ||
    graph_lite_set_int(graph, GRAPH_LITE_EDGE, edge, "weight", 3) != GRAPH_LITE_OK) {
    graph_lite_graph_destroy(graph);
    return 1;
}
graph_lite_remove_node(graph, second); /* removes the edge too */
graph_lite_graph_destroy(graph);
```

```cpp
// C++ — same library, nicer surface, nothing extra exported
#include "graph-lite.hpp"

graph_lite::Graph graph;
const auto first = graph.add_node(1);
const auto second = graph.add_node(2);
const auto edge = graph.add_edge(first, second, 7);
graph.set_int(GRAPH_LITE_EDGE, edge, "weight", 3);
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
