// The typed node/edge store, exercised through the C header only — the same
// surface every foreign binding sees. Nothing under tests/graph-lite/ may name a
// caller's domain concept. Guard: scripts/lint/graph-lite-vocabulary.ts
//
// Ids, tags, edges, listing buffers and the node-removal cascade live here;
// properties are in model_properties_test.cpp.
#include "graph-lite.h"

#include "model_test_support.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr graph_lite_tag kMaxTag = UINT64_MAX;

} // namespace

// --- nodes -----------------------------------------------------------------

TEST(Model, EmptyGraphHasNoNodesOrEdges) {
    auto g = make_graph();
    EXPECT_EQ(graph_lite_node_count(g.get()), 0u);
    EXPECT_EQ(graph_lite_edge_count(g.get()), 0u);
}

TEST(Model, DestroyAcceptsNull) {
    graph_lite_graph_destroy(nullptr);
}

TEST(Model, CreateRejectsNullOutParameter) {
    EXPECT_EQ(graph_lite_graph_create(nullptr), GRAPH_LITE_ERR_INVALID_ARGUMENT);
}

TEST(Model, NodeIdsAreNonzeroAndUnique) {
    auto g = make_graph();
    std::vector<graph_lite_id> ids;
    for (int i = 0; i < 100; ++i)
        ids.push_back(add_node(g.get()));
    for (size_t i = 0; i < ids.size(); ++i) {
        EXPECT_NE(ids[i], kInvalidId);
        for (size_t j = i + 1; j < ids.size(); ++j)
            EXPECT_NE(ids[i], ids[j]);
    }
    EXPECT_EQ(graph_lite_node_count(g.get()), 100u);
}

TEST(Model, FirstIdsStartAtOneInSeparateSpaces) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e = add_edge(g.get(), a, b);
    EXPECT_EQ(a, 1u);
    EXPECT_EQ(b, 2u);
    EXPECT_EQ(e, 1u); // edges count from their own counter, not the node one
}

TEST(Model, AddNodeRejectsNullOutParameter) {
    auto g = make_graph();
    EXPECT_EQ(graph_lite_add_node(g.get(), 1, nullptr), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_node_count(g.get()), 0u);
}

TEST(Model, RemoveNodeThenLookupIsNotFound) {
    auto g = make_graph();
    const auto n = add_node(g.get(), 7);
    ASSERT_EQ(graph_lite_remove_node(g.get(), n), GRAPH_LITE_OK);
    EXPECT_EQ(graph_lite_node_count(g.get()), 0u);

    graph_lite_tag tag = 0;
    EXPECT_EQ(graph_lite_node_tag(g.get(), n, &tag), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_remove_node(g.get(), n), GRAPH_LITE_ERR_NOT_FOUND);
}

TEST(Model, InvalidIdIsNotFoundEverywhere) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    graph_lite_tag tag = 0;
    size_t count = 0;
    graph_lite_id edge = 0;
    EXPECT_EQ(graph_lite_node_tag(g.get(), kInvalidId, &tag), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_remove_node(g.get(), kInvalidId), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_remove_edge(g.get(), kInvalidId), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_edge_info(g.get(), kInvalidId, nullptr, nullptr, nullptr),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_out_edges(g.get(), kInvalidId, nullptr, 0, &count),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_add_edge(g.get(), n, kInvalidId, 1, &edge), GRAPH_LITE_ERR_NOT_FOUND);
}

TEST(Model, NodeIdsAreNeverReusedAfterRemoval) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    ASSERT_EQ(graph_lite_remove_node(g.get(), b), GRAPH_LITE_OK); // the newest one
    const auto c = add_node(g.get());
    EXPECT_GT(c, b);
    EXPECT_NE(c, a);

    ASSERT_EQ(graph_lite_remove_node(g.get(), a), GRAPH_LITE_OK); // an older one
    const auto d = add_node(g.get());
    EXPECT_GT(d, c);

    // Removing everything and starting over still does not rewind the counter.
    ASSERT_EQ(graph_lite_remove_node(g.get(), c), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_remove_node(g.get(), d), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_node_count(g.get()), 0u);
    EXPECT_GT(add_node(g.get()), d);
}

// --- tags ------------------------------------------------------------------

TEST(Model, TagsAreOpaqueIntegersWithNoReservedValue) {
    auto g = make_graph();
    const graph_lite_tag tags[] = {0, 1, 42, kMaxTag};
    for (const auto tag : tags) {
        const auto n = add_node(g.get(), tag);
        graph_lite_tag got = 12345;
        ASSERT_EQ(graph_lite_node_tag(g.get(), n, &got), GRAPH_LITE_OK);
        EXPECT_EQ(got, tag);
    }
}

TEST(Model, TagsAreNotUniqueKeys) {
    auto g = make_graph();
    const auto a = add_node(g.get(), 5);
    const auto b = add_node(g.get(), 5);
    EXPECT_NE(a, b);
    EXPECT_EQ(graph_lite_node_count(g.get()), 2u);
}

TEST(Model, EdgeTagsRoundTripIncludingExtremes) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    for (const graph_lite_tag tag : {graph_lite_tag{0}, kMaxTag}) {
        const auto e = add_edge(g.get(), a, b, tag);
        graph_lite_tag got = 99;
        ASSERT_EQ(graph_lite_edge_info(g.get(), e, nullptr, nullptr, &got), GRAPH_LITE_OK);
        EXPECT_EQ(got, tag);
    }
}

// --- edges -----------------------------------------------------------------

TEST(Model, EdgesAreDirected) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e = add_edge(g.get(), a, b, 9);

    graph_lite_id from = 0, to = 0;
    graph_lite_tag tag = 0;
    ASSERT_EQ(graph_lite_edge_info(g.get(), e, &from, &to, &tag), GRAPH_LITE_OK);
    EXPECT_EQ(from, a);
    EXPECT_EQ(to, b);
    EXPECT_EQ(tag, 9u);

    EXPECT_EQ(out_of(g.get(), a), Ids{e});
    EXPECT_EQ(in_of(g.get(), b), Ids{e});
    EXPECT_TRUE(in_of(g.get(), a).empty());
    EXPECT_TRUE(out_of(g.get(), b).empty());
    EXPECT_EQ(graph_lite_edge_count(g.get()), 1u);
}

TEST(Model, EdgeInfoOutParametersAreEachOptional) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e = add_edge(g.get(), a, b, 3);

    graph_lite_id to = 0;
    EXPECT_EQ(graph_lite_edge_info(g.get(), e, nullptr, &to, nullptr), GRAPH_LITE_OK);
    EXPECT_EQ(to, b);
    EXPECT_EQ(graph_lite_edge_info(g.get(), e, nullptr, nullptr, nullptr), GRAPH_LITE_OK);
}

TEST(Model, AddEdgeWithAbsentEndpointIsNotFoundAndAddsNothing) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto gone = add_node(g.get());
    ASSERT_EQ(graph_lite_remove_node(g.get(), gone), GRAPH_LITE_OK);

    graph_lite_id e = 0;
    EXPECT_EQ(graph_lite_add_edge(g.get(), a, gone, 1, &e), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_add_edge(g.get(), gone, a, 1, &e), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_add_edge(g.get(), a, 9999, 1, &e), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_edge_count(g.get()), 0u);
    EXPECT_TRUE(out_of(g.get(), a).empty());
    EXPECT_TRUE(in_of(g.get(), a).empty());
}

TEST(Model, AddEdgeRejectsNullOutParameter) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    EXPECT_EQ(graph_lite_add_edge(g.get(), a, a, 1, nullptr), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_edge_count(g.get()), 0u);
}

TEST(Model, SelfLoopAppearsInBothLists) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto e = add_edge(g.get(), a, a);
    EXPECT_EQ(out_of(g.get(), a), Ids{e});
    EXPECT_EQ(in_of(g.get(), a), Ids{e});
    EXPECT_EQ(graph_lite_edge_count(g.get()), 1u);

    ASSERT_EQ(graph_lite_remove_edge(g.get(), e), GRAPH_LITE_OK);
    EXPECT_TRUE(out_of(g.get(), a).empty());
    EXPECT_TRUE(in_of(g.get(), a).empty());
}

TEST(Model, ParallelEdgesAreDistinctAndListedAscending) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e1 = add_edge(g.get(), a, b, 1);
    const auto e2 = add_edge(g.get(), a, b, 1);
    const auto e3 = add_edge(g.get(), a, b, 2);
    EXPECT_NE(e1, e2);
    EXPECT_EQ(graph_lite_edge_count(g.get()), 3u);
    EXPECT_EQ(out_of(g.get(), a), (Ids{e1, e2, e3}));
    EXPECT_EQ(in_of(g.get(), b), (Ids{e1, e2, e3}));

    // Removing the middle one leaves the others, still ascending.
    ASSERT_EQ(graph_lite_remove_edge(g.get(), e2), GRAPH_LITE_OK);
    EXPECT_EQ(out_of(g.get(), a), (Ids{e1, e3}));
    EXPECT_EQ(in_of(g.get(), b), (Ids{e1, e3}));
}

TEST(Model, RemoveEdgeLeavesEndpointsAndClearsBothLists) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e = add_edge(g.get(), a, b);
    ASSERT_EQ(graph_lite_remove_edge(g.get(), e), GRAPH_LITE_OK);

    EXPECT_EQ(graph_lite_edge_count(g.get()), 0u);
    EXPECT_EQ(graph_lite_node_count(g.get()), 2u);
    EXPECT_TRUE(out_of(g.get(), a).empty());
    EXPECT_TRUE(in_of(g.get(), b).empty());
    EXPECT_EQ(graph_lite_edge_info(g.get(), e, nullptr, nullptr, nullptr),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_remove_edge(g.get(), e), GRAPH_LITE_ERR_NOT_FOUND);
}

TEST(Model, EdgeIdsAreNeverReusedAfterRemoval) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e1 = add_edge(g.get(), a, b);
    const auto e2 = add_edge(g.get(), b, a);
    ASSERT_EQ(graph_lite_remove_edge(g.get(), e2), GRAPH_LITE_OK); // the newest one
    const auto e3 = add_edge(g.get(), a, b);
    EXPECT_GT(e3, e2);

    ASSERT_EQ(graph_lite_remove_edge(g.get(), e1), GRAPH_LITE_OK); // an older one
    const auto e4 = add_edge(g.get(), a, b);
    EXPECT_GT(e4, e3);
}

TEST(Model, EdgeIdsAreNotReusedWhenANodeRemovalDropsThem) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e1 = add_edge(g.get(), a, b);
    ASSERT_EQ(graph_lite_remove_node(g.get(), b), GRAPH_LITE_OK);
    const auto c = add_node(g.get());
    const auto e2 = add_edge(g.get(), a, c);
    EXPECT_GT(e2, e1);
}

// --- listing buffers -------------------------------------------------------

TEST(Model, ListingReportsRequiredCountWhenBufferTooSmall) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e1 = add_edge(g.get(), a, b);
    const auto e2 = add_edge(g.get(), a, b);
    const auto e3 = add_edge(g.get(), a, b);

    size_t count = 0;
    EXPECT_EQ(graph_lite_out_edges(g.get(), a, nullptr, 0, &count),
              GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(count, 3u);

    graph_lite_id two[2] = {0, 0};
    count = 0;
    EXPECT_EQ(graph_lite_out_edges(g.get(), a, two, 2, &count), GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(count, 3u);

    graph_lite_id exact[3] = {0, 0, 0};
    count = 0;
    EXPECT_EQ(graph_lite_out_edges(g.get(), a, exact, 3, &count), GRAPH_LITE_OK);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(exact[0], e1);
    EXPECT_EQ(exact[1], e2);
    EXPECT_EQ(exact[2], e3);

    graph_lite_id roomy[8] = {};
    count = 0;
    EXPECT_EQ(graph_lite_in_edges(g.get(), b, roomy, 8, &count), GRAPH_LITE_OK);
    EXPECT_EQ(count, 3u);

    count = 0;
    EXPECT_EQ(graph_lite_in_edges(g.get(), b, nullptr, 0, &count), GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(count, 3u);
}

TEST(Model, ListingAnEmptyNodeSucceedsWithZeroCapacity) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    size_t count = 99;
    EXPECT_EQ(graph_lite_out_edges(g.get(), a, nullptr, 0, &count), GRAPH_LITE_OK);
    EXPECT_EQ(count, 0u);
    count = 99;
    EXPECT_EQ(graph_lite_in_edges(g.get(), a, nullptr, 0, &count), GRAPH_LITE_OK);
    EXPECT_EQ(count, 0u);
}

TEST(Model, ListingRejectsNullCountAndMissingNodes) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    graph_lite_id buf[1] = {0};
    size_t count = 0;
    EXPECT_EQ(graph_lite_out_edges(g.get(), a, buf, 1, nullptr), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_in_edges(g.get(), a, buf, 1, nullptr), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_out_edges(g.get(), 9999, buf, 1, &count), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_in_edges(g.get(), 9999, buf, 1, &count), GRAPH_LITE_ERR_NOT_FOUND);
}

// --- node removal cascades -------------------------------------------------

// REGRESSION: removing a node must remove every edge touching it. A store that
// only erased the node left edges whose endpoint no longer existed — still
// countable, still readable through edge_info, still listed by the surviving
// endpoint — which is a dangling reference nothing downstream can detect.
TEST(Model, RemoveNodeRemovesEveryIncidentEdge) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto hub = add_node(g.get());
    const auto c = add_node(g.get());
    const auto into_hub = add_edge(g.get(), a, hub);
    const auto from_hub = add_edge(g.get(), hub, c);
    const auto loop = add_edge(g.get(), hub, hub);
    const auto parallel = add_edge(g.get(), a, hub);
    const auto untouched = add_edge(g.get(), a, c);
    ASSERT_EQ(graph_lite_edge_count(g.get()), 5u);

    ASSERT_EQ(graph_lite_remove_node(g.get(), hub), GRAPH_LITE_OK);

    for (const auto gone : {into_hub, from_hub, loop, parallel}) {
        EXPECT_EQ(graph_lite_edge_info(g.get(), gone, nullptr, nullptr, nullptr),
                  GRAPH_LITE_ERR_NOT_FOUND)
            << "edge " << gone << " survived removal of its endpoint";
        EXPECT_EQ(graph_lite_remove_edge(g.get(), gone), GRAPH_LITE_ERR_NOT_FOUND);
    }

    // Absent from the surviving endpoints' lists in both directions.
    EXPECT_EQ(out_of(g.get(), a), Ids{untouched});
    EXPECT_TRUE(in_of(g.get(), a).empty());
    EXPECT_EQ(in_of(g.get(), c), Ids{untouched});
    EXPECT_TRUE(out_of(g.get(), c).empty());

    // The removed node itself is gone, so its lists are unreadable too.
    size_t count = 0;
    EXPECT_EQ(graph_lite_out_edges(g.get(), hub, nullptr, 0, &count), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_in_edges(g.get(), hub, nullptr, 0, &count), GRAPH_LITE_ERR_NOT_FOUND);

    EXPECT_EQ(graph_lite_edge_count(g.get()), 1u);
    EXPECT_EQ(graph_lite_node_count(g.get()), 2u);

    // The survivor is still fully readable.
    graph_lite_id from = 0, to = 0;
    ASSERT_EQ(graph_lite_edge_info(g.get(), untouched, &from, &to, nullptr), GRAPH_LITE_OK);
    EXPECT_EQ(from, a);
    EXPECT_EQ(to, c);
}

TEST(Model, RemoveHighDegreeNodeRemovesAllOutgoingEdges) {
    constexpr size_t kDegree = 4096;
    auto g = make_graph();
    const auto hub = add_node(g.get());
    for (size_t i = 0; i < kDegree; ++i)
        add_edge(g.get(), hub, add_node(g.get()));

    ASSERT_EQ(graph_lite_edge_count(g.get()), kDegree);
    ASSERT_EQ(graph_lite_remove_node(g.get(), hub), GRAPH_LITE_OK);
    EXPECT_EQ(graph_lite_edge_count(g.get()), 0u);
    EXPECT_EQ(graph_lite_node_count(g.get()), kDegree);
}

TEST(Model, RemoveNodeErasesIncidentEdgeProperties) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e = add_edge(g.get(), a, b);
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_EDGE, e, "weight", 4), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_remove_node(g.get(), b), GRAPH_LITE_OK);

    int64_t out = 0;
    EXPECT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_EDGE, e, "weight", &out),
              GRAPH_LITE_ERR_NOT_FOUND);
}
