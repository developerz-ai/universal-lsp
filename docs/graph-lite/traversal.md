# Traversal

> Shipped in v0.0.1-wip through the C ABI in `graph-lite.h` and the C++
> `graph_lite::Graph::traverse` convenience method. Direction-aware filtered
> any-path and unweighted-shortest-path queries ship through the same C ABI.

## The API is programmatic, not declarative

The library exposes native breadth-first and depth-first walks over forward or
reverse adjacency. A traversal is an opaque C handle; opening it snapshots node
ids, then `next` yields them until `GRAPH_LITE_ERR_END`. The C++ wrapper collects
those ids with the same ABI calls.

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

## Order, direction, and filters run inside the walk

`GRAPH_LITE_WALK_BREADTH_FIRST` visits each depth in adjacency order.
`GRAPH_LITE_WALK_DEPTH_FIRST` uses a stack, so the latest admitted adjacent node
is visited first. `FORWARD` follows outgoing edges; `REVERSE` follows incoming
edges. The start node is always admitted at depth zero, even when a node-tag
filter would exclude it.

Filters are primitive traversal arguments, applied as every edge is considered:
the edge tag and then the destination node tag must match before that destination
enters the queue or stack. A rejected edge never expands the frontier. Nodes are
marked discovered at admission, including with self-edges, parallel edges, and
cycles, so every admitted node is yielded at most once.

## Bounded by default in caller code

Unbounded traversal on a dense graph is unbounded work. `UINT64_MAX` means no
depth or result limit; all other values are inclusive limits. Callers on a latency
budget are expected to set both. `graph_lite_traversal_partial` reports true only
when a depth or result limit prevented an otherwise admissible candidate. Filters,
cycles, and exhausted adjacency do not make a complete traversal partial.

## Paths

`graph_lite_any_path` returns an admissible route, and
`graph_lite_shortest_path` returns an admissible route with the fewest edges.
Both follow `FORWARD` outgoing or `REVERSE` incoming adjacency, applying edge and
then destination-node tag filters before a candidate enters the search, exactly as
walks do. The returned node-id sequence starts with `start`, ends with `target`,
and every adjacent pair is connected by an admitted directed edge in the requested
direction. `start == target` returns that one node even when the node filter would
exclude it.

The caller owns the output buffer. Pass `NULL` and zero capacity to obtain the
required node-id count; a too-small buffer returns `GRAPH_LITE_ERR_BUFFER_TOO_SMALL`
without writing it. A null buffer with nonzero capacity is invalid. Missing start or
target nodes return `GRAPH_LITE_ERR_NOT_FOUND`; when both endpoints exist but no
route survives direction and filters, `GRAPH_LITE_ERR_NO_PATH` returns with count
zero. Thus an unreachable target cannot be mistaken for a successful zero-hop path.
