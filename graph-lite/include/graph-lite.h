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
 *   Build phase 1 (v0.0.1-wip): the typed node/edge store ships behind this ABI
 *   — create/destroy, add/remove, counts, degrees and neighbor buffers, with
 *   stable, never-reused ids and both adjacency directions materialized.
 *   Properties, traversal and persistence are later phases; they extend this
 *   contract additively, never by changing it.
 *
 * Licensed under the MIT License. See graph-lite/LICENSE — deliberately more
 * permissive than the engine that consumes it.
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

/* Version string of the linked library, e.g. "0.0.1". Static storage; the caller
 * does not free it. */
const char* graph_lite_version_string(void);

/* Human-readable text for a status code. Static storage, never NULL — an unknown
 * code returns a string saying so rather than NULL, because a binding that has to
 * null-check an error-message getter will eventually forget. */
const char* graph_lite_status_string(graph_lite_status status);

/*
 * The store — build phase 1 of docs/graph-lite/README.md.
 *
 * HANDLES  graph_lite_graph is opaque and caller-owned: created with
 *          graph_lite_graph_create(), released exactly once with
 *          graph_lite_graph_destroy(). NULL is a valid argument only where
 *          stated; anything else that cannot proceed fails with
 *          GRAPH_LITE_ERR_INVALID_ARGUMENT.
 *
 * IDS      graph_lite_id is uint64_t, issued monotonically per store and never
 *          reused after a delete: an id a caller holds can only ever mean the
 *          same entity or nothing at all. Ids start at 1; 0 never names an
 *          entity, so bindings may keep using it as "no id".
 *
 * BUFFERS  Caller-owned, explicitly sized; no query allocates. When a neighbor
 *          query finds more neighbors than capacity, it fails with
 *          GRAPH_LITE_ERR_BUFFER_TOO_SMALL, stores the required count in
 *          *out_count, and leaves the buffer untouched.
 *
 * THREADS  Single-writer. The store holds no lock; concurrent use must be
 *          serialized by the caller.
 */

typedef uint64_t graph_lite_id;

typedef struct graph_lite_graph graph_lite_graph;

/* Create an empty store. Returns NULL only if the allocation failed. */
graph_lite_graph* graph_lite_graph_create(void);

/* Release a store and every id in it. NULL is allowed and does nothing. */
void graph_lite_graph_destroy(graph_lite_graph* graph);

/* Live entity counts. */
graph_lite_status graph_lite_graph_node_count(const graph_lite_graph* graph, uint64_t* out_count);
graph_lite_status graph_lite_graph_edge_count(const graph_lite_graph* graph, uint64_t* out_count);

/* Footprint estimate: the sum of each internal container's capacity times its
 * element size (hash indices counted at bucket granularity, allocator overhead
 * ignored). For capacity planning, not accounting. */
graph_lite_status graph_lite_graph_memory_usage(const graph_lite_graph* graph, uint64_t* out_bytes);

/* Add a node carrying the caller-defined type tag; the fresh id is written to
 * *out_id. */
graph_lite_status graph_lite_node_add(graph_lite_graph* graph, uint64_t type,
                                      graph_lite_id* out_id);

/* Remove a node. Every incident edge, incoming and outgoing, is removed with
 * it: an edge never outlives either endpoint. */
graph_lite_status graph_lite_node_remove(graph_lite_graph* graph, graph_lite_id id);

/* Read back the type tag a node was added with. */
graph_lite_status graph_lite_node_type(const graph_lite_graph* graph, graph_lite_id id,
                                       uint64_t* out_type);

/* Live edge count per direction for one node. */
graph_lite_status graph_lite_node_out_degree(const graph_lite_graph* graph, graph_lite_id id,
                                             uint64_t* out_degree);
graph_lite_status graph_lite_node_in_degree(const graph_lite_graph* graph, graph_lite_id id,
                                            uint64_t* out_degree);

/* Copy neighbor ids into buffer. Order is unspecified and may change after any
 * removal; sort before comparing. buffer may be NULL only when the call would
 * not write into it: a degree-0 node succeeds with *out_count == 0, while
 * buffer == NULL with room for a real neighbor fails INVALID_ARGUMENT — after
 * the capacity check, so NULL buffer with capacity 0 stays the "how many?"
 * probe and fails BUFFER_TOO_SMALL. */
graph_lite_status graph_lite_node_out_neighbors(const graph_lite_graph* graph, graph_lite_id id,
                                                graph_lite_id* buffer, uint64_t capacity,
                                                uint64_t* out_count);
graph_lite_status graph_lite_node_in_neighbors(const graph_lite_graph* graph, graph_lite_id id,
                                               graph_lite_id* buffer, uint64_t capacity,
                                               uint64_t* out_count);

/* Add a directed, typed edge from -> to. There is no schema: self-loops
 * (from == to) and parallel edges are legal. If either endpoint does not exist
 * the call fails NOT_FOUND and the store is unchanged. */
graph_lite_status graph_lite_edge_add(graph_lite_graph* graph, graph_lite_id from, graph_lite_id to,
                                      uint64_t type, graph_lite_id* out_id);

/* Remove one edge by id. Its endpoints are untouched. */
graph_lite_status graph_lite_edge_remove(graph_lite_graph* graph, graph_lite_id id);

/* Read back the type tag an edge was added with. The id of a removed edge is
 * NOT_FOUND forever — including after its endpoints are removed. */
graph_lite_status graph_lite_edge_type(const graph_lite_graph* graph, graph_lite_id id,
                                       uint64_t* out_type);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GRAPH_LITE_H */
