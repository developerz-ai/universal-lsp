/*
 * Compiled as C, not C++, on purpose.
 *
 * The public header claims to be usable from any language with an FFI, and the
 * cheapest honest test of that claim is a C translation unit that includes it,
 * links, and drives every entry point of the surface. A C++-only test would
 * keep passing while a `bool`, a default argument, or a stray `//`-in-a-macro
 * made the header uncompilable for every binding.
 *
 * Shape: one smoke function; every failed assertion returns its own line
 * number, so a non-zero result names the exact check that failed.
 */
#include "graph-lite.h"

#include <stddef.h>

#define REQUIRE(cond)                                                                              \
    do {                                                                                           \
        if (!(cond))                                                                               \
            return __LINE__;                                                                       \
    } while (0)

#define REQUIRE_STATUS(call, want)                                                                 \
    do {                                                                                           \
        if ((call) != (want))                                                                      \
            return __LINE__;                                                                       \
    } while (0)

int graph_lite_c_abi_smoke(void);

int graph_lite_c_abi_smoke(void) {
    const char* text = NULL;

    /* --- released surface: version + status strings ------------------------ */
    int major = -1, minor = -1, patch = -1;
    graph_lite_version(&major, &minor, &patch);
    REQUIRE(major == GRAPH_LITE_VERSION_MAJOR);
    REQUIRE(minor == GRAPH_LITE_VERSION_MINOR);
    REQUIRE(patch == GRAPH_LITE_VERSION_PATCH);
    graph_lite_version(0, 0, 0); /* every out-parameter is optional */
    text = graph_lite_status_string(GRAPH_LITE_ERR_CORRUPT);
    REQUIRE(text != NULL);
    text = graph_lite_status_string((graph_lite_status)9999);
    REQUIRE(text != NULL);
    text = graph_lite_status_string(GRAPH_LITE_DONE); /* appended in v0.1.0 */
    REQUIRE(text != NULL);
    text = graph_lite_status_string(GRAPH_LITE_ERR_UNREACHABLE);
    REQUIRE(text != NULL);
    text = graph_lite_status_string((graph_lite_status)1000000);
    REQUIRE(text != NULL);

    /* --- store lifecycle + argument guards --------------------------------- */
    REQUIRE_STATUS(graph_lite_store_create(0), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    graph_lite_store_destroy(0); /* NULL-safe by contract */

    graph_lite_store* store = 0;
    REQUIRE_STATUS(graph_lite_store_create(&store), GRAPH_LITE_OK);
    REQUIRE(store != 0);

    graph_lite_node_id node = 0;
    REQUIRE_STATUS(graph_lite_store_add_node(0, 1, &node), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_add_node(store, 1, 0), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    uint64_t count = 0;
    REQUIRE_STATUS(graph_lite_store_node_count(0, &count), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_node_count(store, 0), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_edge_count(0, &count), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_edge_count(store, 0), GRAPH_LITE_ERR_INVALID_ARGUMENT);

    /* --- ids from 1, counts, per-id getters -------------------------------- */
    graph_lite_node_id a = 0, b = 0, c = 0, d = 0;
    REQUIRE_STATUS(graph_lite_store_add_node(store, 1, &a), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_add_node(store, 2, &b), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_add_node(store, 1, &c), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_add_node(store, 2, &d), GRAPH_LITE_OK);
    REQUIRE(a == 1 && b == 2 && c == 3 && d == 4);
    REQUIRE_STATUS(graph_lite_store_node_count(store, &count), GRAPH_LITE_OK);
    REQUIRE(count == 4);
    graph_lite_node_type node_type = 0;
    REQUIRE_STATUS(graph_lite_store_node_type(store, b, &node_type), GRAPH_LITE_OK);
    REQUIRE(node_type == 2);
    REQUIRE_STATUS(graph_lite_store_node_type(store, 999, &node_type), GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_store_node_type(store, GRAPH_LITE_NODE_ID_NONE, &node_type),
                   GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_store_node_type(store, b, 0), GRAPH_LITE_ERR_INVALID_ARGUMENT);

    /* --- edges: unknown endpoints, a self-loop, per-id getters ------------- */
    /* diamond a->b, a->c, b->d, c->d; edge type 10 everywhere except c->d = 20 */
    graph_lite_edge_id ac = 0, bd = 0, cd = 0, loop = 0;
    REQUIRE_STATUS(graph_lite_store_add_edge(0, a, b, 10, &ac), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_add_edge(store, a, b, 10, 0), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_add_edge(store, a, 999, 1, &ac), GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_store_add_edge(store, 999, a, 1, &ac), GRAPH_LITE_ERR_NOT_FOUND);
    graph_lite_edge_id ab = 0;
    REQUIRE_STATUS(graph_lite_store_add_edge(store, a, b, 10, &ab), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_add_edge(store, a, c, 10, &ac), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_add_edge(store, b, d, 10, &bd), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_add_edge(store, c, d, 20, &cd), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_add_edge(store, a, a, 20, &loop), GRAPH_LITE_OK);
    REQUIRE(ab == 1 && ac == 2 && bd == 3 && cd == 4 && loop == 5);
    REQUIRE_STATUS(graph_lite_store_edge_count(store, &count), GRAPH_LITE_OK);
    REQUIRE(count == 5);
    graph_lite_edge_type edge_type = 0;
    REQUIRE_STATUS(graph_lite_store_edge_type(store, cd, &edge_type), GRAPH_LITE_OK);
    REQUIRE(edge_type == 20);
    REQUIRE_STATUS(graph_lite_store_edge_type(store, 999, &edge_type), GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_store_edge_type(store, cd, 0), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    graph_lite_node_id from = 0, to = 0;
    REQUIRE_STATUS(graph_lite_store_edge_endpoints(0, bd, &from, &to),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_edge_endpoints(store, bd, &from, &to), GRAPH_LITE_OK);
    REQUIRE(from == b && to == d);
    REQUIRE_STATUS(graph_lite_store_edge_endpoints(store, bd, &from, 0),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_edge_endpoints(store, 999, &from, &to),
                   GRAPH_LITE_ERR_NOT_FOUND);

    /* --- cursor argument guards --------------------------------------------- */
    graph_lite_cursor* cursor = 0;
    graph_lite_edge_type edge_filter[] = {10};
    REQUIRE_STATUS(graph_lite_cursor_open(0, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING, 0, 0,
                                          0, 0, 0, 0, &cursor),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                                          0, 0, 0, 0, 0, 0, 0),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_cursor_open(store, 999, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                                          0, 0, 0, 0, 0, 0, &cursor),
                   GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_cursor_open(store, a, (graph_lite_order)7, GRAPH_LITE_OUTGOING, 0, 0,
                                          0, 0, 0, 0, &cursor),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST,
                                          (graph_lite_direction)9, 0, 0, 0, 0, 0, 0, &cursor),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    /* filters are pointer+count pairs: both set, or neither */
    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                                          0, 0, 0, 1, 0, 0, &cursor),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                                          0, 0, edge_filter, 0, 0, 0, &cursor),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);

    /* --- walk: start node, sequence, depth, arrived_via, DONE ---------------- */
    graph_lite_node_id seen[8] = {0};
    uint64_t depth = 99;
    graph_lite_edge_id via_edge = 1;
    graph_lite_node_id via_from = 0;
    int truncated = -1;
    graph_lite_status status = GRAPH_LITE_OK;
    unsigned steps = 0;

    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                                          0, 0, 0, 0, 0, 0, &cursor),
                   GRAPH_LITE_OK);
    /* before any next(), the start node is current: depth 0, no edge, itself */
    REQUIRE_STATUS(graph_lite_cursor_depth(cursor, &depth), GRAPH_LITE_OK);
    REQUIRE(depth == 0);
    REQUIRE_STATUS(graph_lite_cursor_arrived_via(cursor, &via_edge, &via_from), GRAPH_LITE_OK);
    REQUIRE(via_edge == GRAPH_LITE_EDGE_ID_NONE && via_from == a);
    REQUIRE_STATUS(graph_lite_cursor_truncated(cursor, &truncated), GRAPH_LITE_OK);
    REQUIRE(truncated == 0); /* nothing bounded, nothing cut */
    while ((status = graph_lite_cursor_next(cursor, &seen[steps])) == GRAPH_LITE_OK)
        ++steps;
    REQUIRE(status == GRAPH_LITE_DONE); /* exhaustion is not an error */
    REQUIRE(steps == 4);
    REQUIRE(seen[0] == a && seen[1] == b && seen[2] == c && seen[3] == d);
    REQUIRE(graph_lite_cursor_next(cursor, &seen[0]) == GRAPH_LITE_DONE); /* stays done */
    REQUIRE_STATUS(graph_lite_cursor_depth(cursor, &depth), GRAPH_LITE_OK);
    REQUIRE(depth == 2); /* accessors keep describing the last returned node */
    /* accessors reject a NULL cursor or NULL buffer */
    REQUIRE_STATUS(graph_lite_cursor_next(0, &seen[0]), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_cursor_next(cursor, 0), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_cursor_depth(0, &depth), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_cursor_arrived_via(0, &via_edge, &via_from),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_cursor_truncated(cursor, 0), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    graph_lite_cursor_free(cursor);
    graph_lite_cursor_free(0); /* NULL-safe by contract */

    /* --- DFS preorder, INCOMING direction ------------------------------------ */
    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_DEPTH_FIRST, GRAPH_LITE_OUTGOING, 0,
                                          0, 0, 0, 0, 0, &cursor),
                   GRAPH_LITE_OK);
    steps = 0;
    while (graph_lite_cursor_next(cursor, &seen[steps]) == GRAPH_LITE_OK)
        ++steps;
    REQUIRE(steps == 4);
    REQUIRE(seen[0] == a && seen[1] == b && seen[2] == d && seen[3] == c);
    graph_lite_cursor_free(cursor);

    REQUIRE_STATUS(graph_lite_cursor_open(store, d, GRAPH_LITE_DEPTH_FIRST, GRAPH_LITE_INCOMING, 0,
                                          0, 0, 0, 0, 0, &cursor),
                   GRAPH_LITE_OK);
    steps = 0;
    while (graph_lite_cursor_next(cursor, &seen[steps]) == GRAPH_LITE_OK)
        ++steps;
    REQUIRE(steps == 4);
    REQUIRE(seen[0] == d && seen[1] == b && seen[2] == a && seen[3] == c);
    graph_lite_cursor_free(cursor);

    /* --- bounds and the truncation flag -------------------------------------- */
    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                                          1, 0, 0, 0, 0, 0, &cursor),
                   GRAPH_LITE_OK);
    steps = 0;
    while (graph_lite_cursor_next(cursor, &seen[steps]) == GRAPH_LITE_OK)
        ++steps;
    REQUIRE(steps == 3);
    REQUIRE(seen[0] == a && seen[1] == b && seen[2] == c);
    REQUIRE_STATUS(graph_lite_cursor_truncated(cursor, &truncated), GRAPH_LITE_OK);
    REQUIRE(truncated == 1); /* d was provably reachable at depth 2 */
    graph_lite_cursor_free(cursor);

    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                                          0, 2, 0, 0, 0, 0, &cursor),
                   GRAPH_LITE_OK);
    steps = 0;
    while (graph_lite_cursor_next(cursor, &seen[steps]) == GRAPH_LITE_OK)
        ++steps;
    REQUIRE(steps == 2);
    REQUIRE(seen[0] == a && seen[1] == b);
    REQUIRE_STATUS(graph_lite_cursor_truncated(cursor, &truncated), GRAPH_LITE_OK);
    REQUIRE(truncated == 1);
    graph_lite_cursor_free(cursor);

    /* --- filters: edge types prune the walk, node types the results, and
     *     the start node alone is exempt from the node filter ---------------- */
    REQUIRE_STATUS(graph_lite_cursor_open(store, d, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_INCOMING,
                                          0, 0, edge_filter, 1, 0, 0, &cursor),
                   GRAPH_LITE_OK);
    steps = 0;
    while (graph_lite_cursor_next(cursor, &seen[steps]) == GRAPH_LITE_OK)
        ++steps;
    REQUIRE(steps == 3); /* c->d has type 20: no way in via c */
    REQUIRE(seen[0] == d && seen[1] == b && seen[2] == a);
    REQUIRE_STATUS(graph_lite_cursor_truncated(cursor, &truncated), GRAPH_LITE_OK);
    REQUIRE(truncated == 0); /* filters shape the walk; they cut nothing */
    graph_lite_cursor_free(cursor);

    graph_lite_node_type node_filter[] = {2};
    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                                          0, 0, 0, 0, node_filter, 1, &cursor),
                   GRAPH_LITE_OK);
    steps = 0;
    while (graph_lite_cursor_next(cursor, &seen[steps]) == GRAPH_LITE_OK)
        ++steps;
    REQUIRE(steps == 3);
    REQUIRE(seen[0] == a && seen[1] == b && seen[2] == d); /* c traversed, not returned */
    graph_lite_cursor_free(cursor);

    /* --- mutation under a live cursor: the walk was taken at open ------------ */
    graph_lite_edge_id back_edge = 0;
    REQUIRE_STATUS(graph_lite_cursor_open(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING,
                                          0, 0, 0, 0, 0, 0, &cursor),
                   GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_remove_edge(store, ab), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_add_edge(store, d, a, 30, &back_edge), GRAPH_LITE_OK);
    steps = 0;
    while (graph_lite_cursor_next(cursor, &seen[steps]) == GRAPH_LITE_OK)
        ++steps;
    REQUIRE(steps == 4);
    REQUIRE(seen[0] == a && seen[1] == b && seen[2] == c && seen[3] == d);
    graph_lite_cursor_free(cursor);

    /* --- removal: edge first, then a node with its incident-edge cascade ----- */
    REQUIRE_STATUS(graph_lite_store_remove_edge(0, ab), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_remove_node(0, a), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_store_remove_edge(store, back_edge), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_edge_type(store, back_edge, &edge_type),
                   GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_store_remove_edge(store, back_edge), GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_store_remove_edge(store, loop), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_node_count(store, &count), GRAPH_LITE_OK);
    REQUIRE(count == 4);
    REQUIRE_STATUS(graph_lite_store_edge_count(store, &count), GRAPH_LITE_OK);
    REQUIRE(count == 3); /* ac, bd, cd remain — ab died in the snapshot test */
    /* removing b cascades: a->b and b->d die with it */
    REQUIRE_STATUS(graph_lite_store_remove_node(store, b), GRAPH_LITE_OK);
    REQUIRE_STATUS(graph_lite_store_node_count(store, &count), GRAPH_LITE_OK);
    REQUIRE(count == 3);
    REQUIRE_STATUS(graph_lite_store_edge_count(store, &count), GRAPH_LITE_OK);
    REQUIRE(count == 2); /* ac and cd survive */
    REQUIRE_STATUS(graph_lite_store_remove_node(store, b), GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_store_node_type(store, b, &node_type), GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_store_edge_endpoints(store, ab, &from, &to),
                   GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_store_edge_endpoints(store, bd, &from, &to),
                   GRAPH_LITE_ERR_NOT_FOUND);

    /* ids are never reused: the retired numbers stay retired */
    graph_lite_node_id fresh = 0;
    REQUIRE_STATUS(graph_lite_store_add_node(store, 3, &fresh), GRAPH_LITE_OK);
    REQUIRE(fresh == 5);
    graph_lite_edge_id fresh_edge = 0;
    REQUIRE_STATUS(graph_lite_store_add_edge(store, fresh, c, 40, &fresh_edge), GRAPH_LITE_OK);
    REQUIRE(fresh_edge == 7); /* ab's 1 was not handed out again */

    /* --- shortest path -------------------------------------------------------- */
    uint64_t length = 0;
    uint64_t required = 0;
    graph_lite_node_id path[4] = {0};
    /* the size query: no buffer, capacity 0, answer in *out_required */
    REQUIRE_STATUS(graph_lite_shortest_path(store, fresh, d, 0, 0, 0, 0, &length, &required),
                   GRAPH_LITE_OK);
    REQUIRE(length == 3 && required == 3); /* fresh -> c -> d */
    /* too small: required reported, buffer untouched */
    path[0] = 12345;
    path[1] = 12345;
    REQUIRE_STATUS(graph_lite_shortest_path(store, fresh, d, 0, 0, path, 2, &length, &required),
                   GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    REQUIRE(required == 3);
    REQUIRE(path[0] == 12345 && path[1] == 12345);
    /* fits */
    REQUIRE_STATUS(graph_lite_shortest_path(store, fresh, d, 0, 0, path, 4, &length, &required),
                   GRAPH_LITE_OK);
    REQUIRE(length == 3 && required == 3);
    REQUIRE(path[0] == fresh && path[1] == c && path[2] == d);
    /* from == to: the one-node path */
    REQUIRE_STATUS(graph_lite_shortest_path(store, fresh, fresh, 0, 0, path, 4, &length, &required),
                   GRAPH_LITE_OK);
    REQUIRE(length == 1 && required == 1);
    REQUIRE(path[0] == fresh);
    /* both endpoints exist, nothing connects them */
    REQUIRE_STATUS(graph_lite_shortest_path(store, a, fresh, 0, 0, path, 4, &length, &required),
                   GRAPH_LITE_ERR_UNREACHABLE);
    /* the edge-type filter applies here too */
    graph_lite_edge_type path_filter[] = {40};
    REQUIRE_STATUS(
        graph_lite_shortest_path(store, fresh, d, path_filter, 1, path, 4, &length, &required),
        GRAPH_LITE_ERR_UNREACHABLE);
    /* argument guards */
    /* the edge filter is a pointer+count pair here too — both set, or neither */
    REQUIRE_STATUS(
        graph_lite_shortest_path(store, fresh, d, edge_filter, 0, path, 4, &length, &required),
        GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_shortest_path(store, fresh, d, 0, 1, path, 4, &length, &required),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_shortest_path(store, a, 999, 0, 0, path, 4, &length, &required),
                   GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_shortest_path(store, 999, a, 0, 0, path, 4, &length, &required),
                   GRAPH_LITE_ERR_NOT_FOUND);
    REQUIRE_STATUS(graph_lite_shortest_path(store, a, a, 0, 0, path, 4, 0, &required),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_shortest_path(store, a, a, 0, 0, path, 4, &length, 0),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_shortest_path(store, a, a, 0, 0, 0, 3, &length, &required),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_shortest_path(store, a, a, 0, 0, path, 0, &length, &required),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);
    REQUIRE_STATUS(graph_lite_shortest_path(0, a, a, 0, 0, path, 4, &length, &required),
                   GRAPH_LITE_ERR_INVALID_ARGUMENT);

    graph_lite_store_destroy(store);
    graph_lite_store_destroy(0);
    return 0;
}
