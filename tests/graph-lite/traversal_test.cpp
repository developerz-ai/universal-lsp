#include "graph-lite.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

using Id = graph_lite_id;

Id add_node(graph_lite_graph* graph, graph_lite_tag tag = 0) {
    Id id = 0;
    EXPECT_EQ(graph_lite_add_node(graph, tag, &id), GRAPH_LITE_OK);
    return id;
}

void add_edge(graph_lite_graph* graph, Id from, Id to, graph_lite_tag tag = 0) {
    Id id = 0;
    EXPECT_EQ(graph_lite_add_edge(graph, from, to, tag, &id), GRAPH_LITE_OK);
}

struct GraphDeleter {
    void operator()(graph_lite_graph* graph) const { graph_lite_graph_destroy(graph); }
};

std::unique_ptr<graph_lite_graph, GraphDeleter> make_graph() {
    graph_lite_graph* graph = nullptr;
    EXPECT_EQ(graph_lite_graph_create(&graph), GRAPH_LITE_OK);
    return std::unique_ptr<graph_lite_graph, GraphDeleter>(graph);
}

struct TraversalDeleter {
    void operator()(graph_lite_traversal* traversal) const {
        graph_lite_traversal_destroy(traversal);
    }
};

std::vector<Id> walk(const graph_lite_graph* graph, Id start, graph_lite_walk_order order,
                     graph_lite_walk_direction direction, uint64_t depth = UINT64_MAX,
                     uint64_t results = UINT64_MAX, int node_filter = 0,
                     graph_lite_tag node_tag = 0, int edge_filter = 0, graph_lite_tag edge_tag = 0,
                     bool* partial = nullptr) {
    graph_lite_traversal* raw = nullptr;
    EXPECT_EQ(graph_lite_traversal_open(graph, start, order, direction, node_filter, node_tag,
                                        edge_filter, edge_tag, depth, results, &raw),
              GRAPH_LITE_OK);
    std::unique_ptr<graph_lite_traversal, TraversalDeleter> traversal(raw);
    std::vector<Id> ids;
    for (Id id = 0; graph_lite_traversal_next(traversal.get(), &id) == GRAPH_LITE_OK;)
        ids.push_back(id);
    if (partial) {
        int value = 0;
        EXPECT_EQ(graph_lite_traversal_partial(traversal.get(), &value), GRAPH_LITE_OK);
        *partial = value != 0;
    }
    return ids;
}

TEST(Traversal, BreadthFirstAndDepthFirstYieldDistinctNodes) {
    auto graph = make_graph();
    const Id a = add_node(graph.get());
    const Id b = add_node(graph.get());
    const Id c = add_node(graph.get());
    const Id d = add_node(graph.get());
    add_edge(graph.get(), a, b);
    add_edge(graph.get(), a, c);
    add_edge(graph.get(), b, d);
    add_edge(graph.get(), c, d);
    EXPECT_EQ(walk(graph.get(), a, GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_FORWARD),
              (std::vector<Id>{a, b, c, d}));
    EXPECT_EQ(walk(graph.get(), a, GRAPH_LITE_WALK_DEPTH_FIRST, GRAPH_LITE_WALK_FORWARD),
              (std::vector<Id>{a, c, d, b}));
}

TEST(Traversal, ReverseDirectionUsesIncomingEdges) {
    auto graph = make_graph();
    const Id a = add_node(graph.get());
    const Id b = add_node(graph.get());
    const Id c = add_node(graph.get());
    add_edge(graph.get(), a, b);
    add_edge(graph.get(), c, b);
    EXPECT_EQ(walk(graph.get(), b, GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_REVERSE),
              (std::vector<Id>{b, a, c}));
}

TEST(Traversal, FiltersStopExcludedEdgesFromExpanding) {
    auto graph = make_graph();
    const Id a = add_node(graph.get(), 1);
    const Id b = add_node(graph.get(), 2);
    const Id c = add_node(graph.get(), 1);
    const Id d = add_node(graph.get(), 1);
    add_edge(graph.get(), a, b, 7);
    add_edge(graph.get(), b, d, 7);
    add_edge(graph.get(), a, c, 8);
    EXPECT_EQ(walk(graph.get(), a, GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_FORWARD,
                   UINT64_MAX, UINT64_MAX, 1, 1),
              (std::vector<Id>{a, c}));
    EXPECT_EQ(walk(graph.get(), a, GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_FORWARD,
                   UINT64_MAX, UINT64_MAX, 0, 0, 1, 7),
              (std::vector<Id>{a, b, d}));
}

TEST(Traversal, LimitsReportPartialOnlyWhenTheyBlockCandidates) {
    auto graph = make_graph();
    const Id a = add_node(graph.get());
    const Id b = add_node(graph.get());
    const Id c = add_node(graph.get());
    add_edge(graph.get(), a, b);
    add_edge(graph.get(), b, c);
    bool partial = false;
    EXPECT_EQ(walk(graph.get(), a, GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_FORWARD, 1,
                   UINT64_MAX, 0, 0, 0, 0, &partial),
              (std::vector<Id>{a, b}));
    EXPECT_TRUE(partial);
    EXPECT_EQ(walk(graph.get(), a, GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_FORWARD,
                   UINT64_MAX, 2, 0, 0, 0, 0, &partial),
              (std::vector<Id>{a, b}));
    EXPECT_TRUE(partial);
    EXPECT_EQ(walk(graph.get(), a, GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_FORWARD,
                   UINT64_MAX, UINT64_MAX, 0, 0, 0, 0, &partial),
              (std::vector<Id>{a, b, c}));
    EXPECT_FALSE(partial);
}

TEST(Traversal, CyclesSelfEdgesAndDenseComponentsTerminateOncePerNode) {
    auto graph = make_graph();
    std::vector<Id> ids;
    for (int index = 0; index < 12; ++index)
        ids.push_back(add_node(graph.get()));
    for (Id from : ids)
        for (Id to : ids)
            add_edge(graph.get(), from, to);
    for (const auto order : {GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_DEPTH_FIRST})
        for (const auto direction : {GRAPH_LITE_WALK_FORWARD, GRAPH_LITE_WALK_REVERSE}) {
            const std::vector<Id> result = walk(graph.get(), ids.front(), order, direction);
            EXPECT_EQ(result.size(), ids.size());
            std::vector<Id> sorted = result;
            std::sort(sorted.begin(), sorted.end());
            EXPECT_EQ(sorted, ids);
        }
}

TEST(Traversal, CppWrapperCollectsResultsAndPartialStatus) {
    graph_lite::Graph graph;
    const Id a = graph.add_node(1);
    const Id b = graph.add_node(1);
    const Id c = graph.add_node(1);
    graph.add_edge(a, b, 1);
    graph.add_edge(b, c, 1);
    const graph_lite::Graph::Walk result =
        graph.traverse(a, GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_FORWARD, 1);
    EXPECT_EQ(result.ids, (std::vector<Id>{a, b}));
    EXPECT_TRUE(result.partial);
}

} // namespace
