// Deep store/walk semantics that are awkward to pin down in the C harness —
// exact sequences, snapshot behaviour, filter interaction — expressed purely
// in the generic model. Nothing here may name a caller's domain concept.
// Guard: scripts/lint/graph-lite-vocabulary.ts
#include "graph-lite.hpp"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace {

using graph_lite::Cursor;
using graph_lite::shortest_path;
using graph_lite::Store;
using Nodes = std::vector<graph_lite_node_id>;

/* Diamond, edges added in this order: a->b, a->c, b->d, c->d. Edge type 10
 * everywhere except c->d = 20. Node types: a, c = 1; b, d = 2. Insertion
 * order breaks every tie below. */
struct Diamond {
    Store store;
    graph_lite_node_id a = 0;
    graph_lite_node_id b = 0;
    graph_lite_node_id c = 0;
    graph_lite_node_id d = 0;
    graph_lite_edge_id ab = 0;
    graph_lite_edge_id ac = 0;
    graph_lite_edge_id bd = 0;

    Diamond()
        : a(store.add_node(1)), b(store.add_node(2)), c(store.add_node(1)), d(store.add_node(2)) {
        ab = store.add_edge(a, b, 10);
        ac = store.add_edge(a, c, 10);
        bd = store.add_edge(b, d, 10);
        store.add_edge(c, d, 20);
    }
};

Nodes collect(Cursor& cursor) {
    Nodes nodes;
    graph_lite_node_id id = 0;
    while (cursor.next(id))
        nodes.push_back(id);
    return nodes;
}

TEST(Model, BreadthFirstVisitsByLevelThenInsertionOrder) {
    Diamond g;
    Cursor cursor(g.store, g.a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING);
    EXPECT_EQ(collect(cursor), Nodes({g.a, g.b, g.c, g.d}));
}

TEST(Model, DepthFirstVisitsInPreorder) {
    Diamond g;
    Cursor cursor(g.store, g.a, GRAPH_LITE_DEPTH_FIRST, GRAPH_LITE_OUTGOING);
    EXPECT_EQ(collect(cursor), Nodes({g.a, g.b, g.d, g.c}));
}

TEST(Model, IncomingWalksTheReversedEdges) {
    Diamond g;
    Cursor breadth(g.store, g.d, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_INCOMING);
    EXPECT_EQ(collect(breadth), Nodes({g.d, g.b, g.c, g.a}));
    Cursor depth(g.store, g.d, GRAPH_LITE_DEPTH_FIRST, GRAPH_LITE_INCOMING);
    EXPECT_EQ(collect(depth), Nodes({g.d, g.b, g.a, g.c}));
}

TEST(Model, StartIsCurrentAtDepthZeroAndArrivedViaNothing) {
    Diamond g;
    Cursor cursor(g.store, g.a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING);
    EXPECT_EQ(cursor.depth(), 0u);
    const auto arrival = cursor.arrived_via();
    EXPECT_EQ(arrival.edge, graph_lite::kEdgeIdNone);
    EXPECT_EQ(arrival.from, g.a);
    graph_lite_node_id id = 0;
    ASSERT_TRUE(cursor.next(id));
    EXPECT_EQ(id, g.a);
    EXPECT_EQ(cursor.depth(), 0u);
    ASSERT_TRUE(cursor.next(id));
    EXPECT_EQ(id, g.b);
    EXPECT_EQ(cursor.depth(), 1u);
    const auto via = cursor.arrived_via();
    EXPECT_EQ(via.edge, g.ab);
    EXPECT_EQ(via.from, g.a);
}

TEST(Model, DepthBoundTruncatesOnlyWhatIsProvablyCut) {
    Diamond g;
    Cursor bounded(g.store, g.a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING, 1);
    EXPECT_EQ(collect(bounded), Nodes({g.a, g.b, g.c}));
    EXPECT_TRUE(bounded.truncated());
    Cursor exact(g.store, g.a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING, 2);
    EXPECT_EQ(collect(exact), Nodes({g.a, g.b, g.c, g.d}));
    EXPECT_FALSE(exact.truncated()); // the bound never bound
}

TEST(Model, ResultBoundTruncatesOnlyWhenMoreWasReachable) {
    Diamond g;
    Cursor cut(g.store, g.a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING, 0, 2);
    EXPECT_EQ(collect(cut), Nodes({g.a, g.b}));
    EXPECT_TRUE(cut.truncated());
    /* a walk that fits its bound exactly: full answer, no flag */
    Store chain;
    const auto first = chain.add_node(1);
    const auto second = chain.add_node(1);
    chain.add_edge(first, second, 1);
    Cursor whole(chain, first, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING, 0, 2);
    EXPECT_EQ(collect(whole), Nodes({first, second}));
    EXPECT_FALSE(whole.truncated());
}

TEST(Model, EdgeTypeFilterSelectsWhichEdgesExist) {
    Diamond g;
    const graph_lite_edge_type keep[] = {10};
    Cursor cursor(g.store, g.d, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_INCOMING, 0, 0, keep, 1);
    EXPECT_EQ(collect(cursor), Nodes({g.d, g.b, g.a})); // c->d is type 20: no way in via c
    EXPECT_FALSE(cursor.truncated());                   // filters shape the walk; they cut nothing
}

TEST(Model, NodeTypeFilterSelectsResultsAndTheWalkPassesThrough) {
    Diamond g;
    const graph_lite_node_type keep[] = {2};
    Cursor cursor(g.store, g.a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING, 0, 0, nullptr, 0,
                  keep, 1);
    /* a is exempt; c (type 1) is traversed through but never returned */
    EXPECT_EQ(collect(cursor), Nodes({g.a, g.b, g.d}));
}

TEST(Model, CursorIsASnapshotOfWhenItOpened) {
    Diamond g;
    Cursor cursor(g.store, g.a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING);
    g.store.remove_node(g.b);       // drops a->b and b->d under the live cursor
    g.store.add_edge(g.c, g.a, 10); // an edge the walk never saw
    EXPECT_EQ(collect(cursor), Nodes({g.a, g.b, g.c, g.d}));
}

TEST(Model, IdsAreStableAndNeverReused) {
    Store store;
    const auto first = store.add_node(1);
    const auto first_edge = store.add_edge(first, first, 1); // self-loop: legal
    store.remove_node(first);                                // drops the self-loop
    const auto second = store.add_node(1);
    const auto second_edge = store.add_edge(second, second, 1);
    EXPECT_EQ(first, 1u);
    EXPECT_EQ(first_edge, 1u);
    EXPECT_EQ(second, 2u); // the freed number was not handed out again
    EXPECT_EQ(second_edge, 2u);
}

TEST(Model, SelfLoopsAndParallelEdgesAreWalkedOnce) {
    Store store;
    const auto a = store.add_node(1);
    const auto b = store.add_node(1);
    store.add_edge(a, a, 1);
    store.add_edge(a, b, 1);
    store.add_edge(a, b, 1); // parallel to the previous edge
    store.add_edge(b, b, 1);
    Cursor cursor(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING);
    EXPECT_EQ(collect(cursor), Nodes({a, b}));
    EXPECT_FALSE(cursor.truncated());
}

TEST(Model, RemovingANodeDropsItsIncidentEdges) {
    Diamond g;
    g.store.remove_node(g.b); // a->b and b->d die with it
    EXPECT_EQ(g.store.node_count(), 3u);
    EXPECT_EQ(g.store.edge_count(), 2u);
    EXPECT_THROW(g.store.edge_type(g.ab), graph_lite::Error);
    EXPECT_THROW(g.store.edge_type(g.bd), graph_lite::Error);
    EXPECT_EQ(g.store.edge_type(g.ac), 10u); // untouched edges stay readable
}

TEST(Model, UnknownAndRemovedIdsAreNotFound) {
    Diamond g;
    EXPECT_THROW(g.store.add_edge(g.a, 999, 10), graph_lite::Error);
    try {
        g.store.node_type(999);
        FAIL() << "a stale id must be NOT_FOUND, not a read of someone else's slot";
    } catch (const graph_lite::Error& e) {
        EXPECT_EQ(e.status(), GRAPH_LITE_ERR_NOT_FOUND);
    }
}

TEST(Model, ShortestPathCountsNodesAndPrefersTheShortRoute) {
    Diamond g;
    EXPECT_EQ(shortest_path(g.store, g.a, g.d), Nodes({g.a, g.b, g.d}));
    EXPECT_EQ(shortest_path(g.store, g.a, g.a), Nodes({g.a})); // from == to: one node
}

TEST(Model, ShortestPathReportsUnreachableRatherThanGuessing) {
    Store store;
    const auto a = store.add_node(1);
    const auto b = store.add_node(1);
    try {
        static_cast<void>(shortest_path(store, a, b));
        FAIL() << "no edge connects these nodes";
    } catch (const graph_lite::Error& e) {
        EXPECT_EQ(e.status(), GRAPH_LITE_ERR_UNREACHABLE);
    }
}

TEST(Model, ShortestPathIsDirectedOutgoing) {
    Store store;
    const auto a = store.add_node(1);
    const auto b = store.add_node(1);
    store.add_edge(b, a, 1); // points the wrong way on purpose
    EXPECT_THROW(shortest_path(store, a, b), graph_lite::Error);
    EXPECT_EQ(shortest_path(store, b, a), Nodes({b, a}));
}

} // namespace
