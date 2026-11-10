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
 *   Typed nodes and typed, directed edges; properties are designed, not yet
 *   part of this surface. Every domain concept a caller has is a node type or
 *   an edge type
 *   that the CALLER defines and names; none of them appear here. Keeping this
 *   header free of any caller's vocabulary is what makes the library reusable and
 *   cheap to extract, and the rule is enforced mechanically rather than by review
 *   discipline — see scripts/lint/graph-lite-vocabulary.ts.
 *
 * STATUS
 *   v0.1.0 ships the node/edge model, in-memory storage and the traversal API
 *   — everything from "Node/edge model, storage and traversal" down. Node/edge
 *   properties and the persistence format remain designed, not shipped:
 *   docs/graph-lite/README.md.
 *
 * Licensed under the MIT License. See graph-lite/LICENSE — deliberately more
 * permissive than the engine that consumes it.
 */
#ifndef GRAPH_LITE_H
#define GRAPH_LITE_H

#include <stdint.h>

#define GRAPH_LITE_VERSION_MAJOR 0
#define GRAPH_LITE_VERSION_MINOR 1
#define GRAPH_LITE_VERSION_PATCH 0
#define GRAPH_LITE_VERSION_STRING "0.1.0"

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

    /* Appended in v0.1.0; the numbers are the ABI and keep their meaning
     * forever. 8 = a cursor is exhausted (not an error); 9 = both endpoints
     * of a requested path exist but nothing connects them. */
    GRAPH_LITE_DONE = 8,
    GRAPH_LITE_ERR_UNREACHABLE = 9,

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
void graph_lite_version(int* major, int* minor, int* patch);

/* Version string of the linked library — its GRAPH_LITE_VERSION_STRING. Static
 * storage; the caller does not free it. */
const char* graph_lite_version_string(void);

/* Human-readable text for a status code. Static storage, never NULL — an unknown
 * code returns a string saying so rather than NULL, because a binding that has to
 * null-check an error-message getter will eventually forget. */
const char* graph_lite_status_string(graph_lite_status status);

/* -------------------------------------------------------------------------
 * Node/edge model, storage and traversal (v0.1.0).
 *
 * ERROR MODEL — for every entry point below, except where noted:
 *   ERR_INVALID_ARGUMENT — a required handle or out-parameter is NULL; a
 *     filter is malformed (array NULL with a non-zero count, or non-NULL
 *     with count 0); an order or direction value that is not one of its
 *     enumerators; or a shortest-path buffer/capacity pair that does not
 *     match (NULL buffer with non-zero capacity, or a buffer with 0).
 *   ERR_NOT_FOUND — an id that is not in the store: never existed, was
 *     removed, or is 0 — GRAPH_LITE_NODE_ID_NONE / GRAPH_LITE_EDGE_ID_NONE
 *     are never valid ids.
 *   ERR_OUT_OF_MEMORY — an allocation inside the call failed. No exception
 *     ever crosses this boundary, and every id already returned stays valid.
 *     A call reporting OOM may still have partially applied a multi-step
 *     update (e.g. part of remove_node's edge cascade): on OOM, inspect the
 *     store rather than assuming it matches the pre-call state.
 *
 * IDS — assigned from 1 and never reused within a store, so an id a caller
 *   still holds can never come to name a different object. 0 is reserved as
 *   the "..._NONE" value above.
 *
 * OWNERSHIP — the caller owns every buffer and every handle; the library
 *   never returns a pointer the caller must free. Handles come in
 *   create/destroy and open/free pairs; destroy and free accept NULL.
 *
 * CONCURRENCY — single-writer. Simultaneous calls touching one store are
 *   the caller's problem.
 * ------------------------------------------------------------------------- */

/* Opaque by contract: no layout is published, so a binding cannot grow a
 * dependency on one and the implementation can change underneath it. */
typedef struct graph_lite_store graph_lite_store;
typedef struct graph_lite_cursor graph_lite_cursor;

/* The fixed-width id and type vocabulary. Their widths are themselves ABI:
 * a binding that read 64 bits and now reads 32 does not fail to compile —
 * it reads garbage. */
typedef uint64_t graph_lite_node_id;
typedef uint64_t graph_lite_edge_id;
typedef uint32_t graph_lite_node_type;
typedef uint32_t graph_lite_edge_type;

#define GRAPH_LITE_NODE_ID_NONE ((graph_lite_node_id)0)
#define GRAPH_LITE_EDGE_ID_NONE ((graph_lite_edge_id)0)

/* Walk shape. BREADTH_FIRST visits by level — all of depth 1 before any of
 * depth 2. DEPTH_FIRST visits in preorder — each node before its
 * descendants. */
typedef enum graph_lite_order {
    GRAPH_LITE_BREADTH_FIRST = 0,
    GRAPH_LITE_DEPTH_FIRST = 1
} graph_lite_order;

/* Which edges to follow out of each node. */
typedef enum graph_lite_direction {
    GRAPH_LITE_OUTGOING = 0,
    GRAPH_LITE_INCOMING = 1
} graph_lite_direction;

/* --- store ------------------------------------------------------------- */

graph_lite_status graph_lite_store_create(graph_lite_store** out_store);
void graph_lite_store_destroy(graph_lite_store* store);

/* Adds a node of `type` and writes its id to *out_id. */
graph_lite_status graph_lite_store_add_node(graph_lite_store* store, graph_lite_node_type type,
                                            graph_lite_node_id* out_id);

/* Adds a directed edge of `type` between two existing nodes — either
 * direction, or from == to (a self-loop) — and writes its id to *out_id. */
graph_lite_status graph_lite_store_add_edge(graph_lite_store* store, graph_lite_node_id from,
                                            graph_lite_node_id to, graph_lite_edge_type type,
                                            graph_lite_edge_id* out_id);

/* Removes a node. Its incident edges are dropped with it: they answer
 * ERR_NOT_FOUND afterwards, and their ids are never reassigned. */
graph_lite_status graph_lite_store_remove_node(graph_lite_store* store, graph_lite_node_id id);

graph_lite_status graph_lite_store_remove_edge(graph_lite_store* store, graph_lite_edge_id id);

graph_lite_status graph_lite_store_node_count(const graph_lite_store* store, uint64_t* out_count);
graph_lite_status graph_lite_store_edge_count(const graph_lite_store* store, uint64_t* out_count);

graph_lite_status graph_lite_store_node_type(const graph_lite_store* store, graph_lite_node_id id,
                                             graph_lite_node_type* out_type);
graph_lite_status graph_lite_store_edge_type(const graph_lite_store* store, graph_lite_edge_id id,
                                             graph_lite_edge_type* out_type);

/* Writes where a live edge points: *out_from -> *out_to. */
graph_lite_status graph_lite_store_edge_endpoints(const graph_lite_store* store,
                                                  graph_lite_edge_id id,
                                                  graph_lite_node_id* out_from,
                                                  graph_lite_node_id* out_to);

/* --- traversal ---------------------------------------------------------- */

/* Performs the whole walk NOW and returns a snapshot cursor: the walk is
 * complete before this call returns, so later store mutation cannot change
 * it, and the cursor may outlive the store.
 *
 * `start` must exist. It is always part of the walk — the cursor's first
 * node, at depth 0, arrived via no edge — and it alone is exempt from the
 * node-type filter. Edges are followed in the order they were added; every
 * node appears at most once.
 *
 * Filters come in pointer+count pairs and must be either both set or both
 * empty. An edge-type filter decides which edges exist for the walk; a
 * node-type filter decides which walked nodes are RETURNED — the walk still
 * passes through the rest.
 *
 * `max_depth` and `max_results`: 0 = unbounded, otherwise nodes past the
 * depth or beyond the result count are cut. graph_lite_cursor_truncated()
 * reports 1 exactly when such a bound provably dropped reachable nodes;
 * filters never count as cuts. */
graph_lite_status graph_lite_cursor_open(const graph_lite_store* store, graph_lite_node_id start,
                                         graph_lite_order order, graph_lite_direction direction,
                                         uint64_t max_depth, uint64_t max_results,
                                         const graph_lite_edge_type* edge_types,
                                         uint64_t edge_type_count,
                                         const graph_lite_node_type* node_types,
                                         uint64_t node_type_count, graph_lite_cursor** out_cursor);

/* Writes the next node of the walk to *out_node: GRAPH_LITE_OK, or
 * GRAPH_LITE_DONE once every node has been returned — not an error, and
 * further calls keep returning DONE. */
graph_lite_status graph_lite_cursor_next(graph_lite_cursor* cursor, graph_lite_node_id* out_node);

/* Depth and arrival of the node cursor_next() returned LAST; before the
 * first call that is the start node — depth 0, *out_edge
 * GRAPH_LITE_EDGE_ID_NONE, *out_from == start. They keep describing that
 * last node after DONE. */
graph_lite_status graph_lite_cursor_depth(const graph_lite_cursor* cursor, uint64_t* out_depth);
graph_lite_status graph_lite_cursor_arrived_via(const graph_lite_cursor* cursor,
                                                graph_lite_edge_id* out_edge,
                                                graph_lite_node_id* out_from);

/* Writes 1 when a depth or result bound cut reachable nodes out of this
 * walk, else 0. */
graph_lite_status graph_lite_cursor_truncated(const graph_lite_cursor* cursor, int* out_truncated);

/* Releases a cursor from graph_lite_cursor_open. Accepts NULL. */
void graph_lite_cursor_free(graph_lite_cursor* cursor);

/* --- shortest path ------------------------------------------------------ */

/* Breadth-first shortest path over OUTGOING edges between two existing
 * nodes, honouring the edge-type filter (NULL+0 = any edge). The path is
 * node ids, `from` first, and *out_length counts NODES: from == to gives a
 * one-node path. *out_required always receives the number of nodes the path
 * needs. On success with capacity >= that, the path is written and OK is
 * returned. With out_path NULL and capacity 0 this is the size query: no
 * buffer is touched, *out_required carries the answer, OK is returned. With
 * a buffer too small, *out_required is set, out_path is left untouched, and
 * ERR_BUFFER_TOO_SMALL is returned. Both nodes existing but nothing
 * connecting them: ERR_UNREACHABLE. */
graph_lite_status graph_lite_shortest_path(const graph_lite_store* store, graph_lite_node_id from,
                                           graph_lite_node_id to,
                                           const graph_lite_edge_type* edge_types,
                                           uint64_t edge_type_count, graph_lite_node_id* out_path,
                                           uint64_t capacity, uint64_t* out_length,
                                           uint64_t* out_required);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GRAPH_LITE_H */
