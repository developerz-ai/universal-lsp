# Traversal

> Design, not shipped behaviour.

## The API is programmatic, not declarative

v1 exposes C++ traversal calls: breadth-first, depth-first, bounded and unbounded
depth, reverse-edge walks, shortest-or-any path between two nodes, and type
filters applied *during* the walk rather than after it.

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
and result limits; callers on a latency budget are expected to use them. A
traversal that cannot finish inside a budget must return a partial result the
caller can recognise as partial — never a truncated one it cannot distinguish from
a complete answer.
