# graph-lite

A small, fast, dependency-free, embeddable property-graph library. Typed nodes,
typed directed edges, arbitrary properties on both. One public header, one static
library, no server, no query language, no runtime dependencies.

**The public surface is a C ABI**, so anything with an FFI can embed it — C, C++,
Ruby, JavaScript, Python, Rust, Go. C++ callers get a header-only ergonomics layer
on top; nobody else has to care that the implementation is C++.

> **v0.1.0 — first release.** The node/edge model ships as an in-memory store:
> stable ids, tags, adjacency listing and typed properties. Traversal, CSR storage
> and persistence are design, not shipped behaviour.

## Two headers, one contract

| Header | For | Is |
|---|---|---|
| `graph-lite.h` | C, and every FFI | **The contract.** C linkage, opaque handles, integer status codes, caller-owned buffers. |
| `graph-lite.hpp` | C++ | Header-only ergonomics over the C header — real types, exceptions. Exports no entry point of its own. |

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

One build note those snippets imply: the default build is a **static** library.
An FFI that dlopens needs a shared one — configure with `-DBUILD_SHARED_LIBS=ON`
and load the resulting `libgraph-lite.so` (`.dylib` / `.dll` elsewhere).

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

The in-memory model, and only that, ships today:

- Add and remove nodes and edges. Removing a node removes its incident edges.
- Read a node's tag, or an edge's endpoints and tag.
- List the ids of the edges leaving or entering a node, ascending.
- Ask for the live node and edge counts.
- Store typed properties — int64, double, bool, string — on nodes and edges.

**Designed, not shipped:** breadth-first and depth-first walks, reverse-edge
walks, shortest-or-any path, type filters during a walk, CSR storage and
persistence. The design lives in [docs/graph-lite](../docs/graph-lite/); none of
it is in the header yet.

## What it deliberately is not

| Not | Why |
|---|---|
| A query language | A programmatic API first. Which queries matter is what real usage answers; designing a language up front commits to guesses. |
| Backed by a relational engine | Recursive-CTE traversal cannot keep visited-set state across iterations — benchmarked 100×+ slower on deep or dense graphs. |
| Distributed, clustered, or server-mode | Embedded and in-process, permanently. |
| Schema-enforcing | Enforce at your own seam if you want it; every writer should not pay for a check most callers do not want. |

## Correctness

What ships is what is tested. Every claim above is pinned by a suite that runs on
every change — and again under ASan + UBSan in CI:

- `tests/graph-lite/c_abi_test.c` is compiled **as C** and linked into the suite,
  so the header cannot silently stop being usable from C — and from every FFI.
- The linked library's version and version string are asserted equal to the
  header's macros, so a header/binary mismatch fails here, not in an embedder.
- `graph_lite_status_string()` is asserted to never return NULL, including for a
  code the linked library does not know.
- Model and property suites cover ids, cascading removal, adjacency listing and
  every property type.

## License

MIT — see [LICENSE](LICENSE). Versioned independently of anything it happens to
be built alongside.
