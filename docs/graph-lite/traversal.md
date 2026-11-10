# Traversal

> Shipped in v0.1.0 as the cursor-based C ABI — no callbacks into the caller's
> runtime: breadth-first by level or depth-first preorder, either direction,
> type filters, depth and result bounds, a truncation flag, and shortest path
> with required-size reporting. The parameterised C++ traversal calls remain
> design.

## The API is programmatic, not declarative

Shipped surface, v0.1.0: `graph_lite_cursor_open` walks eagerly and returns a
snapshot cursor — later mutation cannot change a walk already taken — and
`graph_lite_cursor_next` yields its nodes in order. Breadth-first by level or
depth-first preorder, outgoing or incoming direction, bounded or unbounded
depth, edge and node type filters applied *during* the walk rather than after
it, a truncation flag for bounded walks, and `graph_lite_shortest_path`
between two nodes with required-size reporting. No callbacks into the caller's
runtime anywhere in that surface. The parameterised C++ traversal-call shape
remains design.

No Cypher, no Datalog, no SQL. A query language is a large, permanent surface, and
which queries actually matter is exactly what the first year of real usage
answers. Designing it up front means committing to guesses; adding it later costs
nothing that is not already paid.

## Why traversal is native, and stays native

Relational engines answer reachability with recursive CTEs, and they cannot
maintain visited-node state efficiently across iterations — so each level re-scans
work the previous level already did. Published benchmarking and real-world
migrations consistently show 100×-plus slowdowns on deep or highly-connected
graphs versus an in-memory adjacency walk.

This is a **rejected alternative, not a deferred one**. The core traversal engine
must not be backed by a relational store, however convenient that would be for the
persistence layer.

## Filters run inside the walk

Filtering by node type or edge type after collecting a frontier means paying for
nodes the caller never wanted — and on a highly-connected graph the frontier is
where all the cost is. Filters are arguments to the traversal, applied as edges
are considered.

## Bounded by default in caller code

Unbounded traversal on a dense graph is unbounded work. The library provides depth
and result limits (0 = unbounded); callers on a latency budget are expected to
use them. A traversal that cannot finish inside a budget must return a partial
result the caller can recognise as partial — never a truncated one it cannot
distinguish from a complete answer. Shipped as the cursor's truncation flag
(`graph_lite_cursor_truncated`): 1 only when a bound provably cut reachable
nodes out of the walk.
