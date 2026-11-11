#include "graph-lite.h"

int main(void) {
    graph_lite_graph* graph = 0;
    graph_lite_id first = GRAPH_LITE_INVALID_ID;
    graph_lite_id second = GRAPH_LITE_INVALID_ID;
    graph_lite_id edge = GRAPH_LITE_INVALID_ID;
    int64_t weight = 0;
    int result = 1;

    if (graph_lite_graph_create(&graph) != GRAPH_LITE_OK || graph == 0)
        return result;
    if (graph_lite_add_node(graph, 1, &first) != GRAPH_LITE_OK ||
        graph_lite_add_node(graph, 2, &second) != GRAPH_LITE_OK ||
        graph_lite_add_edge(graph, first, second, 7, &edge) != GRAPH_LITE_OK ||
        graph_lite_set_int(graph, GRAPH_LITE_EDGE, edge, "weight", 3) != GRAPH_LITE_OK ||
        graph_lite_get_int(graph, GRAPH_LITE_EDGE, edge, "weight", &weight) != GRAPH_LITE_OK ||
        weight != 3 || graph_lite_remove_node(graph, second) != GRAPH_LITE_OK ||
        graph_lite_edge_count(graph) != 0)
        goto cleanup;

    result = 0;
cleanup:
    graph_lite_graph_destroy(graph);
    return result;
}
