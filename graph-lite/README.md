# graph-lite

A small, fast, dependency-free, embeddable property-graph library. Typed nodes,
typed directed edges, arbitrary properties on both. One public header, one static
library, no server, no query language, no runtime dependencies.

**The public surface is a C ABI**, so anything with an FFI can embed it — C, C++,
Ruby, JavaScript, Python, Rust, Go. C++ callers get a header-only ergonomics layer
on top; nobody else has to care that the implementation is C++.

> **v0.1.0 — model, storage and traversal shipped.** The C surface covers the
> store (typed nodes and edges, add/remove with a removal cascade, counts,
> type and endpoint accessors), the snapshot cursor (breadth-first by level or
> depth-first preorder, either direction, type filters, depth and result
> bounds, truncation flag) and shortest path with required-size reporting.
> Properties and persistence remain designed, not shipped.

## Two headers, one contract

| Header | For | Is |
|---|---|---|
| `graph-lite.h` | C, and every FFI | **The contract.** C linkage, opaque handles, fixed-width never-reused ids, integer status codes, caller-owned buffers with required-size reporting, no callbacks into the caller's runtime. |
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

## Call it

```c
/* C — and the shape every binding sees */
#include "graph-lite.h"

int major, minor, patch;
graph_lite_version(&major, &minor, &patch);
/* compare against GRAPH_LITE_VERSION_* to catch a header/binary mismatch at
   startup rather than as a corrupt read much later */

graph_lite_store *store = NULL;
graph_lite_store_create(&store);          /* GRAPH_LITE_OK, or a status code */

graph_lite_node_id a, b, edge;
graph_lite_store_add_node(store, 1, &a);  /* ids assigned from 1, never reused */
graph_lite_store_add_node(store, 1, &b);
graph_lite_store_add_edge(store, a, b, 7, &edge);

graph_lite_cursor *walk = NULL;
graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                       /*max_depth*/ 0, /*max_results*/ 0,
                       NULL, 0, NULL, 0, &walk);  /* NULL + 0 filters: any type */
graph_lite_node_id hit;
while (graph_lite_cursor_next(walk, &hit) == GRAPH_LITE_OK) {
    /* visit hit; GRAPH_LITE_DONE ends the walk */
}
graph_lite_cursor_free(walk);
graph_lite_store_destroy(store);
```

```cpp
// C++ — same library, nicer surface, nothing extra exported
#include "graph-lite.hpp"

const auto v = graph_lite::version();
graph_lite::check(some_call());   // throws graph_lite::Error, which carries the code
```

```ruby
# Ruby, via FFI — no build step beyond the shared library
require "ffi"

module GraphLite
  extend FFI::Library
  ffi_lib "graph-lite"
  attach_function :graph_lite_version_string, [], :string
end
```

```js
// JavaScript — Bun's FFI; Node uses N-API or koffi the same way
import { dlopen, FFIType } from "bun:ffi";

const { symbols } = dlopen("libgraph-lite.so", {
  graph_lite_version_string: { args: [], returns: FFIType.cstring },
});
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

- **Node** — a stable id, a type tag, properties.
- **Edge** — directed, from one id to another, a type tag, properties.
- **Type** — an opaque tag *you* define and name. The library attaches no meaning
  to any of them, holds no registry, and reserves no name.

Ids and type tags are in the 0.1.0 ABI; properties are designed but not yet
shipped.

Nothing about your domain appears in this library. That is the design, and it is
enforced by a lint rule rather than left to review.

## What it does

Add and remove nodes and edges. Walk breadth-first or depth-first, bounded or
unbounded. Walk **reverse edges** — the "what points at this" direction — as a
first-class operation. Find the shortest path between two ids, reporting the
buffer size it needs first. Filter by node type and edge type *during* the
walk, not after it.

In memory the store is per-node adjacency vectors plus id->slot maps: insertion
order preserved, removals cheap, nothing rebuilt. CSR — flat contiguous arrays,
cache-friendly to traverse, and close enough to the persisted layout that a
reload is nearly a read rather than a parse-and-rebuild — remains the design
target for the persisted format.

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
