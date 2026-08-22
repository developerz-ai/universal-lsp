# Data model

> Design, not shipped behaviour. v0.0.1 exposes version information only.

## Nodes

A node is an id, a type tag, and a property map.

| Field | Notes |
|---|---|
| id | Stable for the lifetime of the store, including across persistence. Callers hold ids; the library never reuses one after a delete. |
| type | An opaque tag the caller defines. Compared, never interpreted. |
| properties | Arbitrary key/value pairs. |

Ids being stable across a save/load cycle is a **one-way door**: callers will
persist them in their own state the moment the feature exists, so the guarantee
has to be decided before the first release, not after.

Ids cross the C boundary, so their width is ABI. Pick a fixed-width integer type
and never change it — a binding that read 64 bits and now reads 32 does not fail
to compile, it reads garbage.

## Edges

An edge is directed, typed, and carries properties of its own.

| Field | Notes |
|---|---|
| from, to | Node ids. An edge to a deleted node must never be readable. |
| type | An opaque tag, same rules as node types. |
| properties | Arbitrary key/value pairs. |

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

Small, typed values (integers, floats, booleans, strings) keyed by short names.
The property store is separate from the adjacency arrays: traversal must not pull
property bytes into cache lines it does not read.

## What is deliberately absent

| Absent | Why |
|---|---|
| Schema / constraints | A caller that wants them can enforce them at its own seam. Baking them in makes every write pay for a check most callers do not want. |
| Node labels beyond one type tag | Multi-label is a modelling convenience that costs an index. Add it when a real caller cannot express itself without it. |
| A query language | See [traversal.md](traversal.md). |
| Transactions across processes | Embedded, in-process, single-writer for v1. |
