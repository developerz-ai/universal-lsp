#include "graph-lite.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
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
    const Id e = add_node(graph.get(), 1);
    add_edge(graph.get(), a, b, 7);
    add_edge(graph.get(), b, d, 7);
    add_edge(graph.get(), a, c, 8);
    add_edge(graph.get(), c, e, 7);
    EXPECT_EQ(walk(graph.get(), a, GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_FORWARD,
                   UINT64_MAX, UINT64_MAX, 1, 1),
              (std::vector<Id>{a, c, e}));
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

TEST(Traversal, DepthBlockedCandidateAdmittedElsewhereIsNotPartial) {
    auto graph = make_graph();
    const Id a = add_node(graph.get());
    const Id b = add_node(graph.get());
    const Id c = add_node(graph.get());
    const Id d = add_node(graph.get());
    const Id e = add_node(graph.get());
    add_edge(graph.get(), a, b);
    add_edge(graph.get(), a, c);
    add_edge(graph.get(), c, d);
    add_edge(graph.get(), d, e);
    add_edge(graph.get(), b, e);

    bool partial = true;
    EXPECT_EQ(walk(graph.get(), a, GRAPH_LITE_WALK_DEPTH_FIRST, GRAPH_LITE_WALK_FORWARD, 2,
                   UINT64_MAX, 0, 0, 0, 0, &partial),
              (std::vector<Id>{a, c, d, b, e}));
    EXPECT_FALSE(partial);
}

TEST(Traversal, GeneratedCyclesSelfEdgesAndDenseComponentsTerminateOncePerNode) {
    for (size_t size = 1; size <= 16; ++size) {
        auto graph = make_graph();
        std::vector<Id> ids;
        for (size_t index = 0; index < size; ++index)
            ids.push_back(add_node(graph.get()));
        for (size_t from = 0; from < size; ++from) {
            add_edge(graph.get(), ids[from], ids[from]);
            add_edge(graph.get(), ids[from], ids[(from + 1) % size]);
            for (size_t to = 0; to < size; ++to)
                if ((from * 17 + to * 31 + size) % 5 == 0)
                    add_edge(graph.get(), ids[from], ids[to]);
        }

        for (const auto order : {GRAPH_LITE_WALK_BREADTH_FIRST, GRAPH_LITE_WALK_DEPTH_FIRST})
            for (const auto direction : {GRAPH_LITE_WALK_FORWARD, GRAPH_LITE_WALK_REVERSE}) {
                const std::vector<Id> result = walk(graph.get(), ids.front(), order, direction);
                EXPECT_EQ(result.size(), ids.size());
                std::vector<Id> sorted = result;
                std::sort(sorted.begin(), sorted.end());
                EXPECT_EQ(sorted, ids);
            }
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

TEST(Traversal, PathsRespectDirectionFiltersAndShortestHopCount) {
    auto graph = make_graph();
    const Id a = add_node(graph.get(), 1);
    const Id b = add_node(graph.get(), 1);
    const Id c = add_node(graph.get(), 1);
    const Id d = add_node(graph.get(), 1);
    const Id excluded = add_node(graph.get(), 2);
    add_edge(graph.get(), a, b, 7);
    add_edge(graph.get(), b, d, 7);
    add_edge(graph.get(), a, c, 7);
    add_edge(graph.get(), c, d, 7);
    add_edge(graph.get(), a, d, 8);
    add_edge(graph.get(), a, excluded, 7);
    add_edge(graph.get(), excluded, d, 7);

    std::array<Id, 4> path{};
    uint64_t count = 0;
    EXPECT_EQ(graph_lite_shortest_path(graph.get(), a, d, GRAPH_LITE_WALK_FORWARD, 0, 0, 1, 7,
                                       path.data(), path.size(), &count),
              GRAPH_LITE_OK);
    EXPECT_EQ(std::vector<Id>(path.begin(), path.begin() + count), (std::vector<Id>{a, b, d}));
    EXPECT_EQ(graph_lite_any_path(graph.get(), d, a, GRAPH_LITE_WALK_REVERSE, 1, 1, 1, 7,
                                  path.data(), path.size(), &count),
              GRAPH_LITE_OK);
    EXPECT_EQ(path.front(), d);
    EXPECT_EQ(path[count - 1], a);
}

TEST(Traversal, PathsReportNoPathAndKeepBufferContract) {
    auto graph = make_graph();
    const Id a = add_node(graph.get());
    const Id b = add_node(graph.get());
    const Id c = add_node(graph.get());
    add_edge(graph.get(), a, b, 1);
    std::array<Id, 3> path{999, 999, 999};
    uint64_t count = 99;
    EXPECT_EQ(graph_lite_any_path(graph.get(), a, c, GRAPH_LITE_WALK_FORWARD, 0, 0, 0, 0,
                                  path.data(), path.size(), &count),
              GRAPH_LITE_ERR_NO_PATH);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(path.front(), 999);
    EXPECT_EQ(graph_lite_shortest_path(graph.get(), a, b, GRAPH_LITE_WALK_FORWARD, 0, 0, 0, 0,
                                       nullptr, 0, &count),
              GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(count, 2);
}

TEST(Traversal, GeneratedPathsOnlyUseAllowedEdges) {
    for (size_t size = 2; size <= 12; ++size) {
        auto graph = make_graph();
        std::vector<Id> ids;
        for (size_t i = 0; i < size; ++i)
            ids.push_back(add_node(graph.get(), i % 3));
        for (size_t from = 0; from < size; ++from)
            for (size_t to = 0; to < size; ++to)
                if ((from * 7 + to * 11 + size) % 4 == 0)
                    add_edge(graph.get(), ids[from], ids[to], (from + to) % 2);

        std::vector<Id> path(size);
        uint64_t count = 0;
        const graph_lite_status status =
            graph_lite_any_path(graph.get(), ids.front(), ids.back(), GRAPH_LITE_WALK_FORWARD, 0, 0,
                                1, 1, path.data(), path.size(), &count);
        if (status == GRAPH_LITE_ERR_NO_PATH)
            continue;
        ASSERT_EQ(status, GRAPH_LITE_OK);
        ASSERT_GE(count, 1);
        EXPECT_EQ(path.front(), ids.front());
        EXPECT_EQ(path[count - 1], ids.back());
        for (uint64_t index = 1; index < count; ++index) {
            bool connected = false;
            size_t edge_count = 0;
            ASSERT_EQ(graph_lite_out_edges(graph.get(), path[index - 1], nullptr, 0, &edge_count),
                      GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
            std::vector<Id> edges(edge_count);
            ASSERT_EQ(graph_lite_out_edges(graph.get(), path[index - 1], edges.data(), edges.size(),
                                           &edge_count),
                      GRAPH_LITE_OK);
            for (size_t edge_index = 0; edge_index < edge_count; ++edge_index) {
                Id from = 0;
                Id to = 0;
                graph_lite_tag tag = 0;
                ASSERT_EQ(graph_lite_edge_info(graph.get(), edges[edge_index], &from, &to, &tag),
                          GRAPH_LITE_OK);
                connected = connected || (to == path[index] && tag == 1);
            }
            EXPECT_TRUE(connected);
        }
    }
}

} // namespace
