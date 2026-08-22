# graph-lite

A small, fast, dependency-free, embeddable property-graph library for C++20. One
public header, one static library, no server, no query language, no runtime
dependencies.

> **Status: pre-implementation (v0.0.1).** The header currently exposes version
> information only. The model, storage, traversal and persistence described below
> are the design being built — not shipped behaviour. Design docs date from
> 2026-08.

## Why it exists

Embeddable, in-process graph storage is a genuinely unmet need in open source. The
most prominent prior option was abandoned by its maintainers in October 2025,
leaving no actively-maintained, vendor-independent choice. Meanwhile the workloads
that want one — traversal over a few million nodes, at interactive latency, inside
someone else's process — keep being served by a relational engine pretending, and
paying 100× for the pretence (see [traversal](traversal.md)).

graph-lite is the boring answer: flat arrays, native traversal, a file format that
is close enough to the in-memory layout that loading is nearly a read.

## Data model

| Concept | Shape |
|---|---|
| Node | a stable id, a **type** tag, and arbitrary key/value properties |
| Edge | directed, from one node to another, a **type** tag, and properties |
| Type | an opaque tag the caller defines and names. The library attaches no meaning to any of them. |

That is the whole model. There are no built-in types, no schema, no reserved
names. A caller's domain vocabulary lives entirely in the type tags it chooses;
none of it appears in this library, and that is enforced rather than intended —
see [`../architecture/graph-lite-boundary.md`](../architecture/graph-lite-boundary.md).

Full detail: [data-model.md](data-model.md).

## Operations

- Add and remove nodes and edges.
- Traverse breadth-first or depth-first, bounded or unbounded depth.
- Traverse **reverse edges** — the "what points at this" direction, which is the
  expensive one to bolt on later and cheap to design in now.
- Find the shortest or any path between two nodes.
- Filter by node type and edge type during traversal, not after it.

No query language in v1. A programmatic C++ traversal API first; a declarative
layer only once real usage says which queries actually matter, rather than
speculatively. Detail: [traversal.md](traversal.md).

## Storage

A CSR-style (compressed sparse row) adjacency structure: flat, contiguous arrays
for node and edge data. Chosen for two reasons that reinforce each other —
contiguous arrays are cache-friendly to walk, and they map almost directly onto a
persisted file, so a reload is close to a read or an `mmap` rather than a parse
and rebuild. Detail: [persistence.md](persistence.md).

## Concurrency

v1 is single-writer, many-reader. The concurrency model is nevertheless designed
with eventual concurrent multi-writer support in mind: the callers this library is
built for run many independent processes against one store at once, and a hard
single-writer limit was the specific gap that downstream users of the prior art
called out.

## Correctness posture

Silent corruption is the worst thing an embedded storage engine can do — it fails
the one promise that makes embedding acceptable. So the persistence layer gets
disproportionate testing investment relative to the traversal algorithms:

- Fuzzing the reader against malformed and truncated files.
- Crash-consistency testing: interrupt a write at every offset, reopen, assert the
  store is either the old state or the new one and never a third thing.
- The suite is expressed purely in the generic node/edge vocabulary above.

## Embedding

```bash
cmake -S graph-lite -B build/graph-lite -G Ninja
cmake --build build/graph-lite
```

```cpp
#include "graph-lite.h"

const auto v = graph_lite::version();   // check against GRAPH_LITE_VERSION_* macros
```

Or vendor the directory and `add_subdirectory(graph-lite)`. It configures
standalone, so neither path needs anything else from the repository it currently
lives in.

## License and versioning

MIT ([`graph-lite/LICENSE`](../../graph-lite/LICENSE)), versioned independently of
the repository that hosts it. Both are deliberate: embedding one header should
never require reasoning about the license or release cadence of anything else.
