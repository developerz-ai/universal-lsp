// graph-lite standalone smoke test.
//
// Dependency-free: links only graph-lite, uses no GTest, no parent helper, and
// nothing outside graph-lite/. Returns non-zero on the first failure so ctest
// reports it. Compiled as C++ and exercises calls through the C ABI
// (graph-lite.h). C compilation is covered by the root C ABI test.
#include "graph-lite.h"

#include <cstdio>
#include <cstring>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (cond) {
        std::printf("ok   - %s\n", what);
    } else {
        std::printf("FAIL - %s\n", what);
        ++failures;
    }
}

} // namespace

int main() {
    // --- version match / mismatch -----------------------------------------
    int major = 0, minor = 0, patch = 0;
    graph_lite_version(&major, &minor, &patch);
    check(graph_lite_check_version(GRAPH_LITE_VERSION_MAJOR, GRAPH_LITE_VERSION_MINOR,
                                   GRAPH_LITE_VERSION_PATCH) == GRAPH_LITE_OK,
          "graph_lite_check_version matches the header version");
    check(graph_lite_check_version(GRAPH_LITE_VERSION_MAJOR, GRAPH_LITE_VERSION_MINOR,
                                   GRAPH_LITE_VERSION_PATCH + 1) == GRAPH_LITE_ERR_VERSION_MISMATCH,
          "graph_lite_check_version rejects a mismatched patch");

    check(std::strcmp(graph_lite_version_string(), GRAPH_LITE_VERSION_STRING) == 0,
          "graph_lite_version_string matches GRAPH_LITE_VERSION_STRING");

    // --- a small graph through the C ABI ----------------------------------
    graph_lite_graph* g = nullptr;
    check(graph_lite_graph_create(&g) == GRAPH_LITE_OK, "graph_lite_graph_create succeeds");
    check(g != nullptr, "graph_lite_graph_create sets the out handle");

    graph_lite_id a = GRAPH_LITE_INVALID_ID;
    graph_lite_id b = GRAPH_LITE_INVALID_ID;
    graph_lite_id c = GRAPH_LITE_INVALID_ID;
    check(graph_lite_add_node(g, 100, &a) == GRAPH_LITE_OK, "add node a");
    check(graph_lite_add_node(g, 200, &b) == GRAPH_LITE_OK, "add node b");
    check(graph_lite_add_node(g, 300, &c) == GRAPH_LITE_OK, "add node c");
    check(a == 1 && b == 2 && c == 3, "ids are issued from 1, ascending");

    check(graph_lite_node_count(g) == 3, "node count is 3");

    graph_lite_id edge_ab = GRAPH_LITE_INVALID_ID;
    graph_lite_id edge = GRAPH_LITE_INVALID_ID;
    check(graph_lite_add_edge(g, a, b, 10, &edge_ab) == GRAPH_LITE_OK, "add edge a->b");
    check(graph_lite_add_edge(g, b, c, 20, &edge) == GRAPH_LITE_OK, "add edge b->c");
    check(graph_lite_edge_count(g) == 2, "edge count is 2");

    graph_lite_id from = 0, to = 0;
    graph_lite_tag tag = 0;
    check(graph_lite_edge_info(g, edge, &from, &to, &tag) == GRAPH_LITE_OK,
          "graph_lite_edge_info on the last edge");
    check(to == c, "edge_info destination is c");

    // property on a node
    check(graph_lite_set_int(g, GRAPH_LITE_NODE, a, "rank", 7) == GRAPH_LITE_OK,
          "set int property on node a");
    int64_t rank = 0;
    check(graph_lite_get_int(g, GRAPH_LITE_NODE, a, "rank", &rank) == GRAPH_LITE_OK,
          "get int property from node a");
    check(rank == 7, "int property round-trips");

    // adjacency: a -> b -> c
    graph_lite_id buf[8] = {0};
    size_t count = 0;
    check(graph_lite_out_edges(g, a, buf, 8, &count) == GRAPH_LITE_OK && count == 1 &&
              buf[0] == edge_ab,
          "out_edges of a lists the edge to b");
    buf[0] = GRAPH_LITE_INVALID_ID;
    count = 0;
    check(graph_lite_in_edges(g, c, buf, 8, &count) == GRAPH_LITE_OK && count == 1 &&
              buf[0] == edge,
          "in_edges of c lists the edge from b");

    uint64_t degree = 0;
    check(graph_lite_node_out_degree(g, b, &degree) == GRAPH_LITE_OK && degree == 1,
          "out_degree of b is 1");
    degree = 0;
    check(graph_lite_node_in_degree(g, b, &degree) == GRAPH_LITE_OK && degree == 1,
          "in_degree of b is 1");

    // removal cascades
    check(graph_lite_remove_node(g, b) == GRAPH_LITE_OK, "remove node b");
    check(graph_lite_node_count(g) == 2, "node count is 2 after removal");
    check(graph_lite_edge_count(g) == 0, "edge count is 0 after cascaded removal");

    graph_lite_graph_destroy(g);

    // --- invalid argument handling (before destroy) -----------------------
    check(graph_lite_graph_create(nullptr) == GRAPH_LITE_ERR_INVALID_ARGUMENT,
          "create with NULL out is INVALID_ARGUMENT");
    check(graph_lite_add_node(nullptr, 1, &a) == GRAPH_LITE_ERR_INVALID_ARGUMENT,
          "add_node with NULL graph is INVALID_ARGUMENT");

    if (failures == 0) {
        std::printf("standalone_smoke_test: ALL PASS\n");
        return 0;
    }
    std::printf("standalone_smoke_test: %d failure(s)\n", failures);
    return 1;
}
