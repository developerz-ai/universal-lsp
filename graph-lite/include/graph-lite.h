/*
 * graph-lite — a small, dependency-free, embeddable property-graph library.
 *
 * THIS HEADER IS THE CONTRACT. It is C, with C linkage, and it is what every
 * embedder binds against: C, C++, Ruby (FFI), JavaScript (N-API / bun:ffi),
 * Python (ctypes / cffi), Rust, Go, and anything else with an FFI. C++ callers
 * who want ergonomics use graph-lite.hpp, which is header-only and implemented
 * entirely in terms of this file — it exports nothing of its own.
 *
 * That direction is deliberate and not negotiable per-function: C++ has no
 * stable ABI across compilers or standard-library versions, so a C++ signature
 * in a public header is unlinkable from another toolchain and unusable from any
 * non-C++ runtime. See docs/architecture/decisions/0004-graph-lite-c-abi.md.
 *
 * ABI RULES
 *   - No C++ type ever crosses this boundary.
 *   - Handles are opaque. Struct layout in a public header is an ABI promise.
 *   - Failures are integer codes, distinct per reason a caller must distinguish.
 *   - Buffers are caller-owned with an explicit size, or paired with an explicit
 *     free function — an embedder cannot free what a different allocator made.
 *   - Additive only. Changing an exported entry point breaks every binding
 *     silently, at load time.
 *
 * DATA MODEL (generic on purpose)
 *   Typed nodes and typed, directed edges, both carrying arbitrary key/value
 *   properties. Every domain concept a caller has is a node type or an edge type
 *   that the CALLER defines and names; none of them appear here. Keeping this
 *   header free of any caller's vocabulary is what makes the library reusable and
 *   cheap to extract, and the rule is enforced mechanically rather than by review
 *   discipline — see scripts/lint/graph-lite-vocabulary.ts.
 *
 * STATUS
 *   Model phase (v0.0.1-wip): the in-memory node/edge model with typed
 *   properties ships behind this ABI — create/destroy, add/remove, counts,
 *   adjacency listing and property storage — with stable, never-reused ids and
 *   both adjacency directions materialized contiguously (see benchmark.md).
 *   Native traversal ships over the contiguous forward and reverse adjacency.
 *   Persistence remains design — see docs/graph-lite/README.md.
 *
 * Licensed under the MIT License. See graph-lite/LICENSE.
 */
#ifndef GRAPH_LITE_H
#define GRAPH_LITE_H

/* Ids and counts are fixed-width on purpose — their width is ABI, so it comes
 * from <stdint.h> and never from a platform-dependent type. */
#include <stdint.h>

#define GRAPH_LITE_VERSION_MAJOR 0
#define GRAPH_LITE_VERSION_MINOR 0
#define GRAPH_LITE_VERSION_PATCH 1
#define GRAPH_LITE_VERSION_STRING "0.0.1"

#include <stddef.h>

/* Public shared-library entry points. GRAPH_LITE_BUILD is defined only while
 * building graph-lite itself. */
#if defined(_WIN32)
#if defined(GRAPH_LITE_BUILD)
#define GRAPH_LITE_API __declspec(dllexport)
#else
#define GRAPH_LITE_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define GRAPH_LITE_API __attribute__((visibility("default")))
#else
#define GRAPH_LITE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Result codes. Every failure an embedder must tell apart gets its own code —
 * a single "error" collapses reasons that need different handling, and a
 * binding cannot recover a distinction the ABI never made.
 *
 * Additive only: existing values never change meaning.
 */
typedef enum graph_lite_status {
    GRAPH_LITE_OK = 0,
    GRAPH_LITE_ERR_INVALID_ARGUMENT = 1,
    GRAPH_LITE_ERR_OUT_OF_MEMORY = 2,
    GRAPH_LITE_ERR_NOT_FOUND = 3,
    GRAPH_LITE_ERR_IO = 4,
    GRAPH_LITE_ERR_CORRUPT = 5,
    GRAPH_LITE_ERR_VERSION_MISMATCH = 6,
    GRAPH_LITE_ERR_BUFFER_TOO_SMALL = 7,
    /* Iteration is exhausted (graph_lite_cursor_next). Not a failure. */
    GRAPH_LITE_ERR_END = 8,
    /* Both path endpoints exist, but no admissible route connects them. */
    GRAPH_LITE_ERR_NO_PATH = 9,

    /* Forces the enum to int width, which makes the RANGE of the type cover every
     * int a caller could pass. Without it, a value outside the declared
     * enumerators has unspecified behaviour when converted — and a foreign
     * binding can and will pass an unknown code, because it was compiled against
     * a different version of this header. */
    GRAPH_LITE_STATUS_FORCE_INT = 0x7fffffff
} graph_lite_status;

/* Version of the library actually linked, as three out-parameters.
 * Compare against the GRAPH_LITE_VERSION_* macros to catch a header/binary
 * mismatch at startup rather than as a corrupt read much later. Any pointer may
 * be NULL if the caller does not want that component. */
GRAPH_LITE_API
void graph_lite_version(int* major, int* minor, int* patch);

/* Version string of the linked library, e.g. "0.0.1". Static storage; the caller
 * does not free it. */
GRAPH_LITE_API
const char* graph_lite_version_string(void);

/* Human-readable text for a status code. Static storage, never NULL — an unknown
 * code returns a string saying so rather than NULL, because a binding that has to
 * null-check an error-message getter will eventually forget. */
GRAPH_LITE_API
const char* graph_lite_status_string(graph_lite_status status);

/*
 * The model — the node/edge surface of docs/graph-lite/README.md.
 *
 * HANDLES  graph_lite_graph is opaque and caller-owned: created with
 *          graph_lite_graph_create(), released exactly once with
 *          graph_lite_graph_destroy(). NULL is a valid argument only where
 *          stated; anything else that cannot proceed fails with
 *          GRAPH_LITE_ERR_INVALID_ARGUMENT.
 *
 * Ids are 64-bit and 0 is never valid (GRAPH_LITE_INVALID_ID). Node ids and edge
 * ids are SEPARATE spaces, each issued from its own counter starting at 1. A
 * counter never goes down and an id is never reused after removal, so an id a
 * caller holds either names the same thing it always did or is NOT_FOUND.
 *
 * A tag is an opaque 64-bit value the caller defines. It is compared with ==
 * and nothing else: no registry, no reserved value (0 and UINT64_MAX are
 * ordinary tags), no validation.
 *
 * OOM      A mutating call that fails with GRAPH_LITE_ERR_OUT_OF_MEMORY never
 *          leaves the graph half-written: the id it would have issued does not
 *          exist, counts name only entities the graph holds, and every query
 *          answers exactly for what survived. A refused remove_node may
 *          already have drained its incident edges — that drain is
 *          allocation-free and commits first.
 *
 * ------------------------------------------------------------------------ */

/* Opaque handle. One graph is not safe for concurrent mutation; concurrent
 * reads of an unmodified graph are fine. */
typedef struct graph_lite_graph graph_lite_graph;

typedef uint64_t graph_lite_id;
typedef uint64_t graph_lite_tag;

#define GRAPH_LITE_INVALID_ID ((graph_lite_id)0)

/* Which id space an owner id belongs to, for the property entry points. */
typedef enum graph_lite_kind {
    GRAPH_LITE_NODE = 0,
    GRAPH_LITE_EDGE = 1,
    GRAPH_LITE_KIND_FORCE_INT = 0x7fffffff
} graph_lite_kind;

/* The type of value a property currently holds. */
typedef enum graph_lite_value_kind {
    GRAPH_LITE_VALUE_INT = 0,
    GRAPH_LITE_VALUE_DOUBLE = 1,
    GRAPH_LITE_VALUE_BOOL = 2,
    GRAPH_LITE_VALUE_STRING = 3,
    GRAPH_LITE_VALUE_KIND_FORCE_INT = 0x7fffffff
} graph_lite_value_kind;

/* Creates an empty graph. *out is set on success only. Release with
 * graph_lite_graph_destroy. */
GRAPH_LITE_API
graph_lite_status graph_lite_graph_create(graph_lite_graph** out);

/* Releases a graph and everything in it. NULL is accepted and ignored. */
GRAPH_LITE_API
void graph_lite_graph_destroy(graph_lite_graph* g);

/* Adds a node with the given tag; its new id is written to *out_id. */
GRAPH_LITE_API
graph_lite_status graph_lite_add_node(graph_lite_graph* g, graph_lite_tag tag,
                                      graph_lite_id* out_id);

/* Removes a node. Cascades: every incident edge, in either direction, is
 * removed too, along with the properties of the node and of those edges. NOT_FOUND
 * if the id is 0 or absent. */
GRAPH_LITE_API
graph_lite_status graph_lite_remove_node(graph_lite_graph* g, graph_lite_id id);

/* Reads a node's tag. NOT_FOUND if the id is 0 or absent. */
GRAPH_LITE_API
graph_lite_status graph_lite_node_tag(const graph_lite_graph* g, graph_lite_id id,
                                      graph_lite_tag* out_tag);

/* Adds a directed edge from -> to. NOT_FOUND if either endpoint is absent.
 * Self-loops and parallel edges are allowed. */
GRAPH_LITE_API
graph_lite_status graph_lite_add_edge(graph_lite_graph* g, graph_lite_id from, graph_lite_id to,
                                      graph_lite_tag tag, graph_lite_id* out_id);

/* Removes an edge and its properties. NOT_FOUND if the id is 0 or absent. */
GRAPH_LITE_API
graph_lite_status graph_lite_remove_edge(graph_lite_graph* g, graph_lite_id id);

/* Reads an edge's endpoints and tag. Each out pointer is optional (may be NULL).
 * NOT_FOUND if the id is 0 or absent. */
GRAPH_LITE_API
graph_lite_status graph_lite_edge_info(const graph_lite_graph* g, graph_lite_id id,
                                       graph_lite_id* from, graph_lite_id* to, graph_lite_tag* tag);

/* Number of live nodes / edges. 0 for a NULL graph. */
GRAPH_LITE_API
size_t graph_lite_node_count(const graph_lite_graph* g);
GRAPH_LITE_API
size_t graph_lite_edge_count(const graph_lite_graph* g);

/* Lists the ids of the edges leaving / entering a node, ascending, into a
 * caller-owned buffer of cap entries. *count is ALWAYS set to the required size;
 * if cap < required the result is BUFFER_TOO_SMALL and nothing is promised about
 * buf. buf may be NULL when cap == 0 (the size query). A self-loop appears in
 * both lists. NOT_FOUND if the node is 0 or absent. */
GRAPH_LITE_API
graph_lite_status graph_lite_out_edges(const graph_lite_graph* g, graph_lite_id node,
                                       graph_lite_id* buf, size_t cap, size_t* count);
GRAPH_LITE_API
graph_lite_status graph_lite_in_edges(const graph_lite_graph* g, graph_lite_id node,
                                      graph_lite_id* buf, size_t cap, size_t* count);

/* ------------------------------------------------------------------------ *
 * Properties
 *
 * Owned by a node or an edge: (kind, id). Keys are NUL-terminated strings; any
 * non-NULL string is a key, the empty string included. Setting an existing key
 * overwrites it and may change its value kind. Removing a node or edge removes
 * its properties.
 *
 * Errors: NULL pointer arguments or an unknown kind -> INVALID_ARGUMENT; a
 * missing owner or key -> NOT_FOUND; a get_* of the wrong value kind ->
 * INVALID_ARGUMENT.
 * ------------------------------------------------------------------------ */

GRAPH_LITE_API
graph_lite_status graph_lite_set_int(graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                                     const char* key, int64_t value);
GRAPH_LITE_API
graph_lite_status graph_lite_set_double(graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                                        const char* key, double value);
/* Any nonzero value is stored as true. */
GRAPH_LITE_API
graph_lite_status graph_lite_set_bool(graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                                      const char* key, int value);
/* The string is copied. */
GRAPH_LITE_API
graph_lite_status graph_lite_set_string(graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                                        const char* key, const char* value);

GRAPH_LITE_API
graph_lite_status graph_lite_property_kind(const graph_lite_graph* g, graph_lite_kind kind,
                                           graph_lite_id id, const char* key,
                                           graph_lite_value_kind* out);

GRAPH_LITE_API
graph_lite_status graph_lite_get_int(const graph_lite_graph* g, graph_lite_kind kind,
                                     graph_lite_id id, const char* key, int64_t* out);
GRAPH_LITE_API
graph_lite_status graph_lite_get_double(const graph_lite_graph* g, graph_lite_kind kind,
                                        graph_lite_id id, const char* key, double* out);
/* *out is 0 or 1. */
GRAPH_LITE_API
graph_lite_status graph_lite_get_bool(const graph_lite_graph* g, graph_lite_kind kind,
                                      graph_lite_id id, const char* key, int* out);
/* *len is the byte length excluding the NUL, always set on a string property.
 * Success needs cap >= *len + 1 and NUL-terminates buf; otherwise
 * BUFFER_TOO_SMALL. buf may be NULL when cap == 0 (the size query). */
GRAPH_LITE_API
graph_lite_status graph_lite_get_string(const graph_lite_graph* g, graph_lite_kind kind,
                                        graph_lite_id id, const char* key, char* buf, size_t cap,
                                        size_t* len);

GRAPH_LITE_API
graph_lite_status graph_lite_remove_property(graph_lite_graph* g, graph_lite_kind kind,
                                             graph_lite_id id, const char* key);

/* ------------------------------------------------------------------------ *
 * Store queries (additive over the model above)
 *
 * Degrees and neighbor listing read the materialized adjacency directly:
 * both directions are kept contiguous per node (benchmarked in
 * docs/graph-lite/benchmark.md), so these are O(degree) copies. The neighbor
 * functions list NEIGHBOR NODE ids; graph_lite_out_edges /
 * graph_lite_in_edges above list EDGE ids. Queries allocate nothing.
 *
 * THREADS  Single-writer per graph. The store holds no lock; concurrent use
 *          must be serialized by the caller (see the handle note above).
 * ------------------------------------------------------------------------ */

/* Live edge count per direction for one node. NOT_FOUND if the id is 0 or
 * absent. */
GRAPH_LITE_API
graph_lite_status graph_lite_node_out_degree(const graph_lite_graph* graph, graph_lite_id id,
                                             uint64_t* out_degree);
GRAPH_LITE_API
graph_lite_status graph_lite_node_in_degree(const graph_lite_graph* graph, graph_lite_id id,
                                            uint64_t* out_degree);

/* Copy neighbor NODE ids into buffer. Order is unspecified and may change after
 * any removal; sort before comparing. buffer may be NULL only when the call
 * would not write into it: a degree-0 node succeeds with *out_count == 0, while
 * buffer == NULL with room for a real neighbor fails INVALID_ARGUMENT — after
 * the capacity check, so NULL buffer with capacity 0 stays the "how many?"
 * probe and fails BUFFER_TOO_SMALL. */
GRAPH_LITE_API
graph_lite_status graph_lite_node_out_neighbors(const graph_lite_graph* graph, graph_lite_id id,
                                                graph_lite_id* buffer, uint64_t capacity,
                                                uint64_t* out_count);
GRAPH_LITE_API
graph_lite_status graph_lite_node_in_neighbors(const graph_lite_graph* graph, graph_lite_id id,
                                               graph_lite_id* buffer, uint64_t capacity,
                                               uint64_t* out_count);

/* Footprint estimate: the sum of each internal container's capacity times its
 * element size (hash indices counted at bucket granularity, allocator overhead
 * ignored); adjacency and property storage included. For capacity planning,
 * not accounting. */
GRAPH_LITE_API
graph_lite_status graph_lite_graph_memory_usage(const graph_lite_graph* graph, uint64_t* out_bytes);

/* ------------------------------------------------------------------------ *
 * Cursors (additive)
 *
 * A cursor iterates ids through an opaque, caller-owned handle: created with
 * graph_lite_cursor_open(), released exactly once with
 * graph_lite_cursor_destroy(). There are no callbacks.
 *
 * SNAPSHOT  Open copies the ids it will yield. The cursor is independent of
 *           any later mutation of the graph and outlives it: the graph may be
 *           changed or destroyed while a cursor is still being read.
 *
 * ORDER     NODES, EDGES, OUT_EDGES and IN_EDGES yield ids ascending.
 *           OUT_NEIGHBORS and IN_NEIGHBORS yield neighbor NODE ids in
 *           unspecified order, repeating a node once per parallel edge.
 *
 * ------------------------------------------------------------------------ */

/* Opaque handle. One cursor is not safe for concurrent use. */
typedef struct graph_lite_cursor graph_lite_cursor;

/* What a cursor iterates. NODES and EDGES cover the whole graph; the other
 * kinds are relative to one node. */
typedef enum graph_lite_cursor_kind {
    GRAPH_LITE_CURSOR_NODES = 0,
    GRAPH_LITE_CURSOR_EDGES = 1,
    GRAPH_LITE_CURSOR_OUT_EDGES = 2,
    GRAPH_LITE_CURSOR_IN_EDGES = 3,
    GRAPH_LITE_CURSOR_OUT_NEIGHBORS = 4,
    GRAPH_LITE_CURSOR_IN_NEIGHBORS = 5,
    GRAPH_LITE_CURSOR_KIND_FORCE_INT = 0x7fffffff
} graph_lite_cursor_kind;

/* Opens a cursor over a snapshot taken now. `node` is ignored for NODES and
 * EDGES; for the other kinds it names the node, and NOT_FOUND is returned if it
 * is 0 or absent. NULL g or out, or an unknown kind -> INVALID_ARGUMENT.
 * Allocation failure -> OUT_OF_MEMORY. *out is set on success only. */
GRAPH_LITE_API
graph_lite_status graph_lite_cursor_open(const graph_lite_graph* g, graph_lite_cursor_kind kind,
                                         graph_lite_id node, graph_lite_cursor** out);

/* Writes the next id to *out_id. At exhaustion returns GRAPH_LITE_ERR_END and
 * leaves *out_id untouched. NULL c or out_id -> INVALID_ARGUMENT. */
GRAPH_LITE_API
graph_lite_status graph_lite_cursor_next(graph_lite_cursor* c, graph_lite_id* out_id);

/* Copies ALL remaining ids into buf (cap entries). *count is ALWAYS set to the
 * required size. If cap < required the result is BUFFER_TOO_SMALL, nothing is
 * written and the cursor does not advance. On success the cursor is at its
 * end. buf may be NULL when cap == 0 (the size query); NULL buf with cap > 0,
 * or a NULL c or count -> INVALID_ARGUMENT. */
GRAPH_LITE_API
graph_lite_status graph_lite_cursor_drain(graph_lite_cursor* c, graph_lite_id* buf, uint64_t cap,
                                          uint64_t* count);

/* Releases a cursor. NULL is accepted and ignored. */
GRAPH_LITE_API
void graph_lite_cursor_destroy(graph_lite_cursor* c);

/* ------------------------------------------------------------------------ *
 * Traversal
 *
 * A traversal snapshots its result at open, then yields node ids through an
 * opaque handle. It is independent of later graph mutation and may outlive the
 * graph. `max_depth` and `max_results` use UINT64_MAX for no limit.
 *
 * The start node is yielded independently of the node-tag filter at depth 0.
 * For each later candidate, the edge tag and then its destination node tag are
 * checked before it enters the worklist. A nonzero filter flag requires an
 * exact tag match; zero ignores that tag. A node is yielded at most once.
 * ------------------------------------------------------------------------ */

typedef struct graph_lite_traversal graph_lite_traversal;

typedef enum graph_lite_walk_order {
    GRAPH_LITE_WALK_BREADTH_FIRST = 0,
    GRAPH_LITE_WALK_DEPTH_FIRST = 1,
    GRAPH_LITE_WALK_ORDER_FORCE_INT = 0x7fffffff
} graph_lite_walk_order;

typedef enum graph_lite_walk_direction {
    GRAPH_LITE_WALK_FORWARD = 0,
    GRAPH_LITE_WALK_REVERSE = 1,
    GRAPH_LITE_WALK_DIRECTION_FORCE_INT = 0x7fffffff
} graph_lite_walk_direction;

/* Opens and computes a native walk. NULL graph/out, unknown order/direction,
 * or a missing start node -> INVALID_ARGUMENT or NOT_FOUND. Allocation failure
 * -> OUT_OF_MEMORY. *out is set on success only. */
GRAPH_LITE_API graph_lite_status graph_lite_traversal_open(
    const graph_lite_graph* graph, graph_lite_id start, graph_lite_walk_order order,
    graph_lite_walk_direction direction, int filter_node_tag, graph_lite_tag node_tag,
    int filter_edge_tag, graph_lite_tag edge_tag, uint64_t max_depth, uint64_t max_results,
    graph_lite_traversal** out);

/* Writes the next admitted node id; exhaustion returns GRAPH_LITE_ERR_END. */
GRAPH_LITE_API
graph_lite_status graph_lite_traversal_next(graph_lite_traversal* traversal, graph_lite_id* out_id);

/* Writes 1 when a depth or result limit prevented an otherwise admissible
 * candidate, otherwise 0. NULL argument -> INVALID_ARGUMENT. */
GRAPH_LITE_API
graph_lite_status graph_lite_traversal_partial(const graph_lite_traversal* traversal,
                                               int* out_partial);

/* Releases a traversal. NULL is accepted and ignored. */
GRAPH_LITE_API
void graph_lite_traversal_destroy(graph_lite_traversal* traversal);

/* ------------------------------------------------------------------------ *
 * Paths
 *
 * Path queries follow either materialized adjacency direction and apply node
 * and edge tag filters exactly as traversal does: each edge and then its next
 * node must match before it may enter the search. `start` and `target` must
 * both name live nodes; either missing endpoint returns NOT_FOUND. A start
 * equal to target succeeds with that one-node path, regardless of node filter.
 *
 * `buffer` holds node ids from start through target. `out_count` is required
 * and is always set to the required count for a successful path. If capacity
 * is too small, BUFFER_TOO_SMALL is returned and buffer is untouched; buffer
 * may be NULL when capacity is zero for the size query. A NULL buffer with
 * nonzero capacity is INVALID_ARGUMENT. When both endpoints exist but no
 * route survives the filters, NO_PATH is returned and *out_count is zero.
 * ------------------------------------------------------------------------ */

/* Returns one admissible path; its hop count is not specified. */
GRAPH_LITE_API graph_lite_status graph_lite_any_path(const graph_lite_graph* graph,
                                                     graph_lite_id start, graph_lite_id target,
                                                     graph_lite_walk_direction direction,
                                                     int filter_node_tag, graph_lite_tag node_tag,
                                                     int filter_edge_tag, graph_lite_tag edge_tag,
                                                     graph_lite_id* buffer, uint64_t capacity,
                                                     uint64_t* out_count);

/* Returns an admissible path with the fewest edges (unweighted shortest path). */
GRAPH_LITE_API graph_lite_status
graph_lite_shortest_path(const graph_lite_graph* graph, graph_lite_id start, graph_lite_id target,
                         graph_lite_walk_direction direction, int filter_node_tag,
                         graph_lite_tag node_tag, int filter_edge_tag, graph_lite_tag edge_tag,
                         graph_lite_id* buffer, uint64_t capacity, uint64_t* out_count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GRAPH_LITE_H */
