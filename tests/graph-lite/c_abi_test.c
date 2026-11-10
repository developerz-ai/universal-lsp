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

    /* ---- phase-1 store surface, called exactly as an FFI binding would ---- */

    /* Null handle and null out-parameter are distinct from every other failure,
       and are checked before anything is dereferenced. */
    uint64_t value = 0;
    if (graph_lite_graph_node_count(0, &value) != GRAPH_LITE_ERR_INVALID_ARGUMENT)
        return 10;
    if (graph_lite_node_add(0, 7, &value) != GRAPH_LITE_ERR_INVALID_ARGUMENT)
        return 11;

    graph_lite_graph* graph = graph_lite_graph_create();
    if (graph == 0)
        return 12;

    uint64_t a = 0;
    uint64_t b = 0;
    if (graph_lite_node_add(graph, 7, &a) != GRAPH_LITE_OK)
        return 13;
    if (graph_lite_node_add(graph, 8, &b) != GRAPH_LITE_OK)
        return 14;
    if (a == 0 || b == 0 || b <= a)
        return 15; /* ids start at 1 and are issued monotonically */

    uint64_t edge = 0;
    if (graph_lite_edge_add(graph, a, b, 9, &edge) != GRAPH_LITE_OK)
        return 16;
    if (edge == 0)
        return 17; /* edge ids come from their own monotonic space, never 0 */

    value = 0;
    if (graph_lite_node_out_degree(graph, a, &value) != GRAPH_LITE_OK || value != 1)
        return 18;
    if (graph_lite_node_in_degree(graph, b, &value) != GRAPH_LITE_OK || value != 1)
        return 19;

    uint64_t neighbors[1] = {0};
    uint64_t count = 0;
    if (graph_lite_node_out_neighbors(graph, a, neighbors, 1, &count) != GRAPH_LITE_OK ||
        count != 1 || neighbors[0] != b)
        return 20;
    if (graph_lite_node_in_neighbors(graph, b, neighbors, 1, &count) != GRAPH_LITE_OK ||
        count != 1 || neighbors[0] != a)
        return 21;

    /* Capacity below degree: required count comes back, buffer untouched. */
    neighbors[0] = 123;
    if (graph_lite_node_out_neighbors(graph, a, neighbors, 0, &count) !=
            GRAPH_LITE_ERR_BUFFER_TOO_SMALL ||
        count != 1 || neighbors[0] != 123)
        return 22;

    if (graph_lite_node_type(graph, b, &value) != GRAPH_LITE_OK || value != 8)
        return 23;

    /* Removing an endpoint cascades the edge: gone, and the id stays gone. */
    if (graph_lite_node_remove(graph, a) != GRAPH_LITE_OK)
        return 24;
    if (graph_lite_edge_type(graph, edge, &value) != GRAPH_LITE_ERR_NOT_FOUND)
        return 25;
    if (graph_lite_node_out_degree(graph, a, &value) != GRAPH_LITE_ERR_NOT_FOUND)
        return 26;

    uint64_t nodes = 0;
    uint64_t edges = 0;
    if (graph_lite_graph_node_count(graph, &nodes) != GRAPH_LITE_OK || nodes != 1)
        return 27;
    if (graph_lite_graph_edge_count(graph, &edges) != GRAPH_LITE_OK || edges != 0)
        return 28;

    graph_lite_graph_destroy(graph);
    graph_lite_graph_destroy(0); /* NULL destroy is a no-op */

    return 0;
}
