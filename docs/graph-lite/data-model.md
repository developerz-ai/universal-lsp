# Data model

> **Shipped in v0.0.1-wip (in-memory, through the C ABI in `graph-lite.h`):**
> stable ids, type tags, directed edges with first-class reverse adjacency, and
> properties as described here. The property store stays separate from the
> adjacency arrays (see Properties below). Native traversal is shipped; persistence
> remains design.

## Nodes

A node is an id, a type tag, and a property map.

| Field | Notes |
|---|---|
| id | Stable for the lifetime of the store, including across persistence. Callers hold ids; the library never reuses one after a delete. |
| type | An opaque `uint64_t` tag the caller defines. Compared by equality, never interpreted. 0 is an ordinary tag. |
| properties | Arbitrary key/value pairs. |

Ids being stable across a save/load cycle is a **one-way door**: callers will
persist them in their own state the moment the feature exists, so the guarantee
has to be decided before the first release, not after.

Ids cross the C boundary, so their width is ABI. Pick a fixed-width integer type
and never change it — a binding that read 64 bits and now reads 32 does not fail
to compile, it reads garbage.

**Decided:**

- Ids are `uint64_t`. 0 is never a valid id (`GRAPH_LITE_INVALID_ID`); a lookup of
  0 is NOT_FOUND.
- Node ids and edge ids are separate spaces, each issued from its own counter that
  starts at 1, only goes up, and never reuses an id after removal.
- Ids are plain identities with no meaning in their bits, so a later save/load can
  restore them verbatim together with the two counters. No file layout is chosen
  here — see [persistence.md](persistence.md).

## Edges

An edge is directed, typed, and carries properties of its own.

| Field | Notes |
|---|---|
| from, to | Node ids. An edge to a deleted node must never be readable: removing a node removes every incident edge (in both directions) and their properties. |
| type | An opaque tag, same rules as node types. |
| properties | Arbitrary key/value pairs. |

Self-loops and parallel edges are allowed; a self-loop is listed once as outgoing
and once as incoming. Edge ids ascend within each node's lists.

Directed with first-class reverse traversal: for the workloads this library
targets, "what points at this node" is asked as often as "what does this node
point at", and a reverse index retrofitted later is a storage-format change rather
than a feature.

## Types are tags, not schema

The library holds no registry, validates nothing, and reserves no name. Two
callers can use the same tag for different things in different stores and neither
is wrong. This is what keeps the model generic — the moment the library
understands a type, it has an opinion about someone's domain.

## Properties

Small, typed values — `int64`, `double`, `bool`, string — keyed by short names
(any non-NULL string, the empty one included). Setting an existing key overwrites
it and may change its value kind; reading with the wrong kind is an error.
The property store is separate from the adjacency arrays: traversal must not pull
property bytes into cache lines it does not read.

## What is deliberately absent

| Absent | Why |
|---|---|
| Schema / constraints | A caller that wants them can enforce them at its own seam. Baking them in makes every write pay for a check most callers do not want. |
| Node labels beyond one type tag | Multi-label is a modelling convenience that costs an index. Add it when a real caller cannot express itself without it. |
| A query language | See [traversal.md](traversal.md). |
| Transactions across processes | Embedded, in-process, single-writer for v1. |
