/*
 * Compiled as C, not C++, on purpose.
 *
 * The public header claims to be usable from any language with an FFI, and the
 * cheapest honest test of that claim is a C translation unit that includes it and
 * links. A C++-only test would keep passing while a `bool`, a default argument,
 * or a stray `//`-in-a-macro made the header uncompilable for every binding —
 * and nothing would say so until an embedder tried.
 */
#include "graph-lite.h"

int graph_lite_c_abi_smoke(void);

int graph_lite_c_abi_smoke(void) {
    int major = -1, minor = -1, patch = -1;
    const char* text;
    graph_lite_graph* g = 0;
    graph_lite_id a = GRAPH_LITE_INVALID_ID;
    graph_lite_id b = GRAPH_LITE_INVALID_ID;
    graph_lite_id e = GRAPH_LITE_INVALID_ID;
    graph_lite_id from = GRAPH_LITE_INVALID_ID;
    graph_lite_id to = GRAPH_LITE_INVALID_ID;
    graph_lite_tag tag = 0;
    graph_lite_id listed[1];
    size_t count = 99;

    graph_lite_version(&major, &minor, &patch);
    if (major != GRAPH_LITE_VERSION_MAJOR)
        return 1;
    if (minor != GRAPH_LITE_VERSION_MINOR)
        return 2;
    if (patch != GRAPH_LITE_VERSION_PATCH)
        return 3;

    /* Every out-parameter is optional; a binding that wants one component should
       not have to allocate three. */
    graph_lite_version(0, 0, 0);

    text = graph_lite_status_string(GRAPH_LITE_ERR_CORRUPT);
    if (text == 0)
        return 4;

    /* An out-of-range code must still return a string, never NULL. */
    text = graph_lite_status_string((graph_lite_status)9999);
    if (text == 0)
        return 5;

    /* The store: create, add, remove, destroy — through the C surface only. */
    if (graph_lite_graph_create(&g) != GRAPH_LITE_OK || g == 0)
        return 6;
    if (graph_lite_add_node(g, 0, &a) != GRAPH_LITE_OK || a == GRAPH_LITE_INVALID_ID)
        return 7;
    if (graph_lite_add_node(g, 1, &b) != GRAPH_LITE_OK || b == a)
        return 8;
    if (graph_lite_add_edge(g, a, b, 2, &e) != GRAPH_LITE_OK || e == GRAPH_LITE_INVALID_ID)
        return 9;
    if (graph_lite_node_count(g) != 2 || graph_lite_edge_count(g) != 1)
        return 10;
    if (graph_lite_edge_info(g, e, &from, &to, &tag) != GRAPH_LITE_OK || from != a || to != b ||
        tag != 2)
        return 11;
    if (graph_lite_out_edges(g, a, listed, 1, &count) != GRAPH_LITE_OK || count != 1 ||
        listed[0] != e)
        return 12;

    /* Removing a node takes its incident edges with it. */
    if (graph_lite_remove_node(g, b) != GRAPH_LITE_OK)
        return 13;
    if (graph_lite_edge_info(g, e, 0, 0, 0) != GRAPH_LITE_ERR_NOT_FOUND)
        return 14;
    if (graph_lite_node_count(g) != 1 || graph_lite_edge_count(g) != 0)
        return 15;
    if (graph_lite_out_edges(g, a, 0, 0, &count) != GRAPH_LITE_OK || count != 0)
        return 16;
    if (graph_lite_remove_node(g, b) != GRAPH_LITE_ERR_NOT_FOUND)
        return 17;
    graph_lite_graph_destroy(g); /* the model smoke's handle lives only here */

    /* ---- the store-query surface: degrees and neighbor buffers, called
       exactly as an FFI binding would ---- */

    /* Null handle and null out-parameter are distinct from every other failure,
       and are checked before anything is dereferenced. */
    {
        uint64_t value = 0;
        uint64_t ncount = 0;
        graph_lite_graph* graph = 0;
        graph_lite_id edge = GRAPH_LITE_INVALID_ID;

        if (graph_lite_add_node(0, 7, &value) != GRAPH_LITE_ERR_INVALID_ARGUMENT)
            return 20;
        if (graph_lite_graph_create(&graph) != GRAPH_LITE_OK || graph == 0)
            return 21;

        if (graph_lite_add_node(graph, 7, &a) != GRAPH_LITE_OK)
            return 22;
        if (graph_lite_add_node(graph, 8, &b) != GRAPH_LITE_OK)
            return 23;
        if (a == 0 || b == 0 || b <= a)
            return 24; /* ids start at 1 and are issued monotonically */

        if (graph_lite_add_edge(graph, a, b, 9, &edge) != GRAPH_LITE_OK)
            return 25;
        if (edge == 0)
            return 26; /* edge ids come from their own monotonic space, never 0 */

        value = 0;
        if (graph_lite_node_out_degree(graph, a, &value) != GRAPH_LITE_OK || value != 1)
            return 27;
        if (graph_lite_node_in_degree(graph, b, &value) != GRAPH_LITE_OK || value != 1)
            return 28;

        {
            graph_lite_id neighbors[1] = {0};
            if (graph_lite_node_out_neighbors(graph, a, neighbors, 1, &ncount) != GRAPH_LITE_OK ||
                ncount != 1 || neighbors[0] != b)
                return 29;
            if (graph_lite_node_in_neighbors(graph, b, neighbors, 1, &ncount) != GRAPH_LITE_OK ||
                ncount != 1 || neighbors[0] != a)
                return 30;

            /* Capacity below degree: required count comes back, buffer untouched. */
            neighbors[0] = 123;
            if (graph_lite_node_out_neighbors(graph, a, neighbors, 0, &ncount) !=
                    GRAPH_LITE_ERR_BUFFER_TOO_SMALL ||
                ncount != 1 || neighbors[0] != 123)
                return 31;
        }

        if (graph_lite_node_tag(graph, b, &tag) != GRAPH_LITE_OK || tag != 8)
            return 32;

        /* Removing an endpoint cascades the edge: gone, and the id stays gone. */
        if (graph_lite_remove_node(graph, a) != GRAPH_LITE_OK)
            return 33;
        if (graph_lite_edge_info(graph, edge, 0, 0, 0) != GRAPH_LITE_ERR_NOT_FOUND)
            return 34;
        if (graph_lite_node_out_degree(graph, a, &value) != GRAPH_LITE_ERR_NOT_FOUND)
            return 35;

        if (graph_lite_node_count(graph) != 1 || graph_lite_edge_count(graph) != 0)
            return 36;

        graph_lite_graph_destroy(graph);
    }
    graph_lite_graph_destroy(0); /* NULL destroy is a no-op */

    return 0;
}
