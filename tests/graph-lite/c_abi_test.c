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
    graph_lite_id c = GRAPH_LITE_INVALID_ID;
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
    {
        graph_lite_id path[2] = {111, 222};
        uint64_t path_count = 0;
        if (graph_lite_shortest_path(g, a, b, GRAPH_LITE_WALK_FORWARD, 0, 0, 1, 2, 0, 0,
                                     &path_count) != GRAPH_LITE_ERR_BUFFER_TOO_SMALL ||
            path_count != 2)
            return 18;
        if (graph_lite_any_path(g, a, b, GRAPH_LITE_WALK_FORWARD, 0, 0, 1, 2, path, 2,
                                &path_count) != GRAPH_LITE_OK ||
            path_count != 2 || path[0] != a || path[1] != b)
            return 19;
        if (graph_lite_add_node(g, 3, &c) != GRAPH_LITE_OK)
            return 20;
        path_count = 99;
        if (graph_lite_any_path(g, a, c, GRAPH_LITE_WALK_FORWARD, 0, 0, 0, 0, path, 2,
                                &path_count) != GRAPH_LITE_ERR_NO_PATH ||
            path_count != 0 || path[0] != a || path[1] != b)
            return 21;
        if (graph_lite_remove_node(g, c) != GRAPH_LITE_OK)
            return 22;
    }

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

    /* ---- the cursor surface: handles, snapshots, END, and the two-call drain ---- */
    {
        graph_lite_graph* cg = 0;
        graph_lite_cursor* cur = 0;
        graph_lite_cursor* other = 0;
        graph_lite_id n1 = 0, n2 = 0, n3 = 0;
        graph_lite_id e1 = 0, e2 = 0, e3 = 0;
        graph_lite_id got = 0;
        graph_lite_id buf[8];
        uint64_t need = 0;
        int i;

        /* ids and tags are 64-bit on every platform: bindings pin this width. */
        if (sizeof(graph_lite_id) != 8 || sizeof(graph_lite_tag) != 8)
            return 40;
        if ((int)GRAPH_LITE_ERR_END != 8 || (int)GRAPH_LITE_ERR_BUFFER_TOO_SMALL != 7)
            return 41;
        text = graph_lite_status_string(GRAPH_LITE_ERR_END);
        if (text == 0 || text[0] == '\0')
            return 42;
        if (graph_lite_status_string((graph_lite_status)-1) == 0 ||
            graph_lite_status_string((graph_lite_status)0x7fffffff) == 0)
            return 43;

        /* n1 -> n2 twice (parallel edges), n2 -> n3 once. */
        if (graph_lite_graph_create(&cg) != GRAPH_LITE_OK || cg == 0)
            return 44;
        if (graph_lite_add_node(cg, 1, &n1) != GRAPH_LITE_OK ||
            graph_lite_add_node(cg, 1, &n2) != GRAPH_LITE_OK ||
            graph_lite_add_node(cg, 1, &n3) != GRAPH_LITE_OK)
            return 45;
        if (graph_lite_add_edge(cg, n1, n2, 2, &e1) != GRAPH_LITE_OK ||
            graph_lite_add_edge(cg, n1, n2, 2, &e2) != GRAPH_LITE_OK ||
            graph_lite_add_edge(cg, n2, n3, 2, &e3) != GRAPH_LITE_OK)
            return 46;

        /* Argument validation: NULL handles and unknown kinds are INVALID_ARGUMENT,
           and *out is written on success only. */
        if (graph_lite_cursor_open(0, GRAPH_LITE_CURSOR_NODES, 0, &cur) !=
                GRAPH_LITE_ERR_INVALID_ARGUMENT ||
            cur != 0)
            return 47;
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_NODES, 0, 0) !=
            GRAPH_LITE_ERR_INVALID_ARGUMENT)
            return 48;
        if (graph_lite_cursor_open(cg, (graph_lite_cursor_kind)99, 0, &cur) !=
                GRAPH_LITE_ERR_INVALID_ARGUMENT ||
            cur != 0)
            return 49;
        if (graph_lite_cursor_next(0, &got) != GRAPH_LITE_ERR_INVALID_ARGUMENT)
            return 50;
        if (graph_lite_cursor_drain(0, buf, 8, &need) != GRAPH_LITE_ERR_INVALID_ARGUMENT)
            return 51;
        graph_lite_cursor_destroy(0); /* NULL destroy is a no-op */

        /* Adjacency kinds: an absent node is NOT_FOUND, and so is id 0. */
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_OUT_EDGES, n3 + 100, &cur) !=
                GRAPH_LITE_ERR_NOT_FOUND ||
            cur != 0)
            return 52;
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_IN_NEIGHBORS, 0, &cur) !=
                GRAPH_LITE_ERR_NOT_FOUND ||
            cur != 0)
            return 53;

        /* NODES: ascending, then END with *out_id untouched, and END is sticky. */
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_NODES, 0, &cur) != GRAPH_LITE_OK ||
            cur == 0)
            return 54;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_OK || got != n1)
            return 55;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_OK || got != n2)
            return 56;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_OK || got != n3)
            return 57;
        got = 777;
        for (i = 0; i < 2; ++i) {
            if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_ERR_END || got != 777)
                return 58;
        }
        if (graph_lite_cursor_next(cur, 0) != GRAPH_LITE_ERR_INVALID_ARGUMENT)
            return 59;
        graph_lite_cursor_destroy(cur);
        cur = 0;

        /* EDGES: ascending; the node argument is ignored. */
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_EDGES, 12345, &cur) != GRAPH_LITE_OK)
            return 60;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_OK || got != e1)
            return 61;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_OK || got != e2)
            return 62;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_OK || got != e3)
            return 63;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_ERR_END)
            return 64;
        graph_lite_cursor_destroy(cur);
        cur = 0;

        /* OUT_EDGES / IN_EDGES: ascending edge ids. */
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_OUT_EDGES, n1, &cur) != GRAPH_LITE_OK)
            return 65;
        if (graph_lite_cursor_drain(cur, buf, 8, &need) != GRAPH_LITE_OK || need != 2 ||
            buf[0] != e1 || buf[1] != e2)
            return 66;
        graph_lite_cursor_destroy(cur);
        cur = 0;
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_IN_EDGES, n2, &cur) != GRAPH_LITE_OK)
            return 67;
        if (graph_lite_cursor_drain(cur, buf, 8, &need) != GRAPH_LITE_OK || need != 2 ||
            buf[0] != e1 || buf[1] != e2)
            return 68;
        graph_lite_cursor_destroy(cur);
        cur = 0;

        /* OUT_NEIGHBORS / IN_NEIGHBORS: order unspecified, parallel edges repeat. */
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_OUT_NEIGHBORS, n1, &cur) != GRAPH_LITE_OK)
            return 69;
        if (graph_lite_cursor_drain(cur, buf, 8, &need) != GRAPH_LITE_OK || need != 2 ||
            buf[0] != n2 || buf[1] != n2)
            return 70;
        graph_lite_cursor_destroy(cur);
        cur = 0;
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_IN_NEIGHBORS, n2, &cur) != GRAPH_LITE_OK)
            return 71;
        if (graph_lite_cursor_drain(cur, buf, 8, &need) != GRAPH_LITE_OK || need != 2 ||
            buf[0] != n1 || buf[1] != n1)
            return 72;
        graph_lite_cursor_destroy(cur);
        cur = 0;

        /* A node with no out-edges yields an empty cursor, not an error. */
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_OUT_EDGES, n3, &cur) != GRAPH_LITE_OK)
            return 73;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_ERR_END)
            return 74;
        graph_lite_cursor_destroy(cur);
        cur = 0;

        /* Drain size query: NULL/0 reports the required count, writes nothing and
           does not advance. Sentinel buffers prove "writes nothing". */
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_NODES, 0, &cur) != GRAPH_LITE_OK)
            return 75;
        need = 0;
        if (graph_lite_cursor_drain(cur, 0, 0, &need) != GRAPH_LITE_ERR_BUFFER_TOO_SMALL ||
            need != 3)
            return 76;
        for (i = 0; i < 8; ++i)
            buf[i] = 555;
        need = 0;
        if (graph_lite_cursor_drain(cur, buf, 2, &need) != GRAPH_LITE_ERR_BUFFER_TOO_SMALL ||
            need != 3)
            return 77;
        if (buf[0] != 555 || buf[1] != 555 || buf[2] != 555)
            return 78;
        if (graph_lite_cursor_drain(cur, 0, 5, &need) != GRAPH_LITE_ERR_INVALID_ARGUMENT)
            return 79; /* NULL buffer with a non-zero capacity is a caller bug */
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_OK || got != n1)
            return 81; /* none of the failed drains advanced the cursor */

        /* Remaining count after a partial consume, then a correctly sized drain. */
        need = 0;
        if (graph_lite_cursor_drain(cur, 0, 0, &need) != GRAPH_LITE_ERR_BUFFER_TOO_SMALL ||
            need != 2)
            return 82;
        if (graph_lite_cursor_drain(cur, buf, 2, &need) != GRAPH_LITE_OK || need != 2 ||
            buf[0] != n2 || buf[1] != n3)
            return 83;
        if (buf[2] != 555)
            return 84; /* wrote exactly the required count */
        got = 888;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_ERR_END || got != 888)
            return 85;
        /* An exhausted cursor drains to an empty, successful result. */
        need = 99;
        if (graph_lite_cursor_drain(cur, 0, 0, &need) != GRAPH_LITE_OK || need != 0)
            return 86;
        graph_lite_cursor_destroy(cur);
        cur = 0;

        /* Snapshot independence: mutation after open is invisible to the cursor,
           and a second cursor opened afterwards sees the new state. */
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_NODES, 0, &cur) != GRAPH_LITE_OK)
            return 87;
        if (graph_lite_remove_node(cg, n2) != GRAPH_LITE_OK)
            return 88;
        if (graph_lite_add_node(cg, 1, &got) != GRAPH_LITE_OK)
            return 89;
        if (graph_lite_cursor_open(cg, GRAPH_LITE_CURSOR_NODES, 0, &other) != GRAPH_LITE_OK)
            return 90;
        if (graph_lite_cursor_drain(other, buf, 8, &need) != GRAPH_LITE_OK || need != 3 ||
            buf[0] != n1 || buf[1] != n3 || buf[2] != got)
            return 91;
        graph_lite_cursor_destroy(other);
        other = 0;

        /* The cursor also outlives the graph it was opened on. */
        graph_lite_graph_destroy(cg);
        cg = 0;
        if (graph_lite_cursor_drain(cur, buf, 8, &need) != GRAPH_LITE_OK || need != 3 ||
            buf[0] != n1 || buf[1] != n2 || buf[2] != n3)
            return 92;
        if (graph_lite_cursor_next(cur, &got) != GRAPH_LITE_ERR_END)
            return 93;
        graph_lite_cursor_destroy(cur);
    }

    return 0;
}
