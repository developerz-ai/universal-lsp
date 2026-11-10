// The OOM seam, pinned at the only place it can be observed: a caller on the
// far side of the C ABI can neither catch nor unwind a C++ exception, so every
// allocation failure must arrive as GRAPH_LITE_ERR_OUT_OF_MEMORY — or as NULL
// from create — with the store still answering for everything it held before.
// Each sweep forces a refusal at a different allocation point of the call under
// test; deleting the try/catch seam in src/graph.cpp lets bad_alloc escape into
// the test body, which every sweep below reports as a failure. Kept out of
// store_test.cpp on purpose: that file has no line budget left, and this needs
// a process-wide allocator override that should not leak into other suites.
// Guard: scripts/lint/graph-lite-vocabulary.ts
#include "graph-lite.h"

#include "alloc_probe.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

using graph_lite_test::AllocProbe;

using Id = graph_lite_id;

// Far above any call's allocation count; the sweeps stop at the first threshold
// that lets the call complete, and must reach it well inside this bound.
constexpr int kMaxThreshold = 64;

graph_lite_graph* make_two_nodes(Id* id1, Id* id2) {
    graph_lite_graph* g = graph_lite_graph_create();
    EXPECT_NE(g, nullptr);
    if (g == nullptr)
        return nullptr;
    EXPECT_EQ(graph_lite_node_add(g, 10, id1), GRAPH_LITE_OK);
    EXPECT_EQ(graph_lite_node_add(g, 20, id2), GRAPH_LITE_OK);
    return g;
}

// A node must answer exactly this type tag and these two degrees.
void expect_node_answers(graph_lite_graph* g, Id id, uint64_t type, uint64_t out_deg,
                         uint64_t in_deg, int threshold) {
    uint64_t t = 99, od = 99, ind = 99;
    EXPECT_EQ(graph_lite_node_type(g, id, &t), GRAPH_LITE_OK) << "threshold " << threshold;
    EXPECT_EQ(t, type) << "threshold " << threshold;
    EXPECT_EQ(graph_lite_node_out_degree(g, id, &od), GRAPH_LITE_OK) << "threshold " << threshold;
    EXPECT_EQ(od, out_deg) << "threshold " << threshold;
    EXPECT_EQ(graph_lite_node_in_degree(g, id, &ind), GRAPH_LITE_OK) << "threshold " << threshold;
    EXPECT_EQ(ind, in_deg) << "threshold " << threshold;
}

// The node's out (out_side) or in neighbor list must equal `want` exactly — the
// check that catches two nodes claiming the same run cells.
void expect_neighbors(graph_lite_graph* g, Id id, bool out_side, const std::vector<Id>& want,
                      int threshold) {
    std::vector<Id> buffer(want.size() + 4, 0);
    uint64_t count = 99;
    const graph_lite_status st =
        out_side ? graph_lite_node_out_neighbors(g, id, buffer.data(), buffer.size(), &count)
                 : graph_lite_node_in_neighbors(g, id, buffer.data(), buffer.size(), &count);
    EXPECT_EQ(st, GRAPH_LITE_OK) << "threshold " << threshold;
    if (st != GRAPH_LITE_OK)
        return;
    EXPECT_EQ(count, want.size()) << "threshold " << threshold;
    buffer.resize(static_cast<size_t>(count));
    EXPECT_EQ(buffer, want) << "threshold " << threshold;
}

// A refused call must have issued no id — 0 stays "no id", and nothing it names
// may exist — and left both pre-existing nodes exactly as they were: types,
// degrees and neighbor slices all answerable and unchanged.
void expect_edge_refusal_was_clean(graph_lite_graph* g, Id id1, Id id2) {
    uint64_t type = 0;
    EXPECT_EQ(graph_lite_edge_type(g, 0, &type), GRAPH_LITE_ERR_NOT_FOUND);

    uint64_t nodes = 99;
    EXPECT_EQ(graph_lite_graph_node_count(g, &nodes), GRAPH_LITE_OK);
    EXPECT_EQ(nodes, 2u);
    EXPECT_EQ(graph_lite_node_type(g, id1, &type), GRAPH_LITE_OK);
    EXPECT_EQ(type, 10u);
    EXPECT_EQ(graph_lite_node_type(g, id2, &type), GRAPH_LITE_OK);
    EXPECT_EQ(type, 20u);

    uint64_t deg = 99;
    EXPECT_EQ(graph_lite_node_out_degree(g, id1, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 0u);
    EXPECT_EQ(graph_lite_node_in_degree(g, id2, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 0u);

    std::vector<Id> buffer(4, 0);
    uint64_t count = 99;
    EXPECT_EQ(graph_lite_node_out_neighbors(g, id1, buffer.data(), buffer.size(), &count),
              GRAPH_LITE_OK);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(graph_lite_node_in_neighbors(g, id2, buffer.data(), buffer.size(), &count),
              GRAPH_LITE_OK);
    EXPECT_EQ(count, 0u);

    // The record vector and the id index must agree — no orphan edge counted —
    // and the store keeps taking work: the edge lands on retry, removes again.
    uint64_t edges = 99;
    EXPECT_EQ(graph_lite_graph_edge_count(g, &edges), GRAPH_LITE_OK);
    EXPECT_EQ(edges, 0u);
    Id retry = 0;
    ASSERT_EQ(graph_lite_edge_add(g, id1, id2, 8, &retry), GRAPH_LITE_OK);
    EXPECT_EQ(graph_lite_edge_type(g, retry, &type), GRAPH_LITE_OK);
    EXPECT_EQ(type, 8u);
    EXPECT_EQ(graph_lite_edge_remove(g, retry), GRAPH_LITE_OK);
    EXPECT_EQ(graph_lite_graph_edge_count(g, &edges), GRAPH_LITE_OK);
    EXPECT_EQ(edges, 0u);
}

} // namespace

// One graph per threshold: the call must either succeed — every allocation it
// needs got through — or report OOM. Never throw, never touch what it could not
// finish. The sweep walks every allocation point of edge_add: pool growth for
// each direction, the edge record vector, the id index.
TEST(AllocFailure, EdgeAddRefusalReportsOomAndLeavesStoreAnswerable) {
    bool succeeded = false;
    for (int threshold = 1; threshold <= kMaxThreshold && !succeeded; ++threshold) {
        Id id1 = 0;
        Id id2 = 0;
        graph_lite_graph* g = make_two_nodes(&id1, &id2);
        ASSERT_NE(g, nullptr);

        Id edge = 0;
        graph_lite_status st = GRAPH_LITE_OK;
        {
            AllocProbe::Scope refusal(threshold);
            st = graph_lite_edge_add(g, id1, id2, 7, &edge);
        }

        if (st == GRAPH_LITE_OK) {
            succeeded = true;
            uint64_t edges = 0;
            EXPECT_EQ(graph_lite_graph_edge_count(g, &edges), GRAPH_LITE_OK);
            EXPECT_EQ(edges, 1u);
            uint64_t type = 0;
            EXPECT_EQ(graph_lite_edge_type(g, edge, &type), GRAPH_LITE_OK);
            EXPECT_EQ(type, 7u);
        } else {
            EXPECT_EQ(st, GRAPH_LITE_ERR_OUT_OF_MEMORY) << "threshold " << threshold;
            EXPECT_EQ(edge, 0u) << "threshold " << threshold;
            expect_edge_refusal_was_clean(g, id1, id2);
        }
        graph_lite_graph_destroy(g);
    }
    EXPECT_TRUE(succeeded) << "no threshold let edge_add complete its allocations";
}

// The fifth edge into a full initial run cannot widen in place — the pool has
// no gap — so edge_add must relocate the run: reserve fresh cells, copy the
// live prefix, release the old range. A refusal at any step of that sequence
// must leave both runs exact, and the retry must land the edge whole — and
// removable again.
TEST(AllocFailure, EdgeAddRefusalDuringRelocationLeavesRunsExact) {
    bool succeeded = false;
    for (int threshold = 1; threshold <= kMaxThreshold && !succeeded; ++threshold) {
        graph_lite_graph* g = graph_lite_graph_create();
        ASSERT_NE(g, nullptr);
        Id a = 0;
        Id b = 0;
        ASSERT_EQ(graph_lite_node_add(g, 1, &a), GRAPH_LITE_OK);
        ASSERT_EQ(graph_lite_node_add(g, 2, &b), GRAPH_LITE_OK);
        Id edge = 0;
        for (uint64_t i = 0; i < 4; ++i)
            ASSERT_EQ(graph_lite_edge_add(g, a, b, i, &edge), GRAPH_LITE_OK);

        Id fifth = 0;
        graph_lite_status st = GRAPH_LITE_OK;
        {
            AllocProbe::Scope refusal(threshold);
            st = graph_lite_edge_add(g, a, b, 99, &fifth);
        }

        if (st == GRAPH_LITE_OK) {
            succeeded = true;
            expect_node_answers(g, a, 1, 5, 0, threshold);
            expect_node_answers(g, b, 2, 0, 5, threshold);
            expect_neighbors(g, a, true, std::vector<Id>(5, b), threshold);
            expect_neighbors(g, b, false, std::vector<Id>(5, a), threshold);
            uint64_t edges = 99;
            EXPECT_EQ(graph_lite_graph_edge_count(g, &edges), GRAPH_LITE_OK);
            EXPECT_EQ(edges, 5u);
        } else {
            EXPECT_EQ(st, GRAPH_LITE_ERR_OUT_OF_MEMORY) << "threshold " << threshold;
            EXPECT_EQ(fifth, 0u) << "threshold " << threshold;
            expect_node_answers(g, a, 1, 4, 0, threshold);
            expect_node_answers(g, b, 2, 0, 4, threshold);
            expect_neighbors(g, a, true, std::vector<Id>(4, b), threshold);
            expect_neighbors(g, b, false, std::vector<Id>(4, a), threshold);
            // Continue mutating: the retry lands, and the edge it landed
            // removes again cleanly.
            ASSERT_EQ(graph_lite_edge_add(g, a, b, 99, &fifth), GRAPH_LITE_OK)
                << "threshold " << threshold;
            expect_node_answers(g, a, 1, 5, 0, threshold);
            EXPECT_EQ(graph_lite_edge_remove(g, fifth), GRAPH_LITE_OK) << "threshold " << threshold;
            expect_node_answers(g, a, 1, 4, 0, threshold);
            expect_node_answers(g, b, 2, 0, 4, threshold);
        }
        graph_lite_graph_destroy(g);
    }
    EXPECT_TRUE(succeeded) << "no threshold let the relocating edge_add complete";
}

TEST(AllocFailure, NodeAddRefusalReportsOomAndKeepsCounts) {
    bool succeeded = false;
    for (int threshold = 1; threshold <= kMaxThreshold && !succeeded; ++threshold) {
        graph_lite_graph* g = graph_lite_graph_create();
        ASSERT_NE(g, nullptr);

        Id id = 0;
        graph_lite_status st = GRAPH_LITE_OK;
        {
            AllocProbe::Scope refusal(threshold);
            st = graph_lite_node_add(g, 42, &id);
        }

        if (st == GRAPH_LITE_OK) {
            succeeded = true;
            uint64_t nodes = 0;
            EXPECT_EQ(graph_lite_graph_node_count(g, &nodes), GRAPH_LITE_OK);
            EXPECT_EQ(nodes, 1u);
            uint64_t type = 0;
            EXPECT_EQ(graph_lite_node_type(g, id, &type), GRAPH_LITE_OK);
            EXPECT_EQ(type, 42u);
        } else {
            // Not reading the refused id back: the call failed, the id was
            // never issued, and nothing may answer for it.
            EXPECT_EQ(st, GRAPH_LITE_ERR_OUT_OF_MEMORY) << "threshold " << threshold;
            EXPECT_EQ(id, 0u) << "threshold " << threshold;
            uint64_t nodes = 99;
            EXPECT_EQ(graph_lite_graph_node_count(g, &nodes), GRAPH_LITE_OK);
            EXPECT_EQ(nodes, 0u);
            // The id this call would have issued (a fresh store starts ids at
            // 1) must answer nothing, and removing it must be a clean miss —
            // not a read past the end of the record vector through an index
            // entry that points at storage that never arrived.
            uint64_t type = 99;
            EXPECT_EQ(graph_lite_node_type(g, 1, &type), GRAPH_LITE_ERR_NOT_FOUND)
                << "threshold " << threshold;
            EXPECT_EQ(graph_lite_node_remove(g, 1), GRAPH_LITE_ERR_NOT_FOUND)
                << "threshold " << threshold;
            // The store keeps taking work: two more nodes and an edge land
            // whole, degrees included.
            Id first = 0;
            Id second = 0;
            Id edge = 0;
            ASSERT_EQ(graph_lite_node_add(g, 43, &first), GRAPH_LITE_OK)
                << "threshold " << threshold;
            ASSERT_EQ(graph_lite_node_add(g, 44, &second), GRAPH_LITE_OK)
                << "threshold " << threshold;
            ASSERT_EQ(graph_lite_edge_add(g, first, second, 7, &edge), GRAPH_LITE_OK)
                << "threshold " << threshold;
            expect_node_answers(g, first, 43, 1, 0, threshold);
            expect_node_answers(g, second, 44, 0, 1, threshold);
        }
        graph_lite_graph_destroy(g);
    }
    EXPECT_TRUE(succeeded) << "no threshold let node_add complete its allocations";
}

// Incident edges drain before node removal allocates anything, so even a
// refused removal must leave the invariant intact: an edge never outlives an
// endpoint.
TEST(AllocFailure, NodeRemoveRefusalReportsOomAndStillDrainsIncidentEdges) {
    bool succeeded = false;
    for (int threshold = 1; threshold <= kMaxThreshold && !succeeded; ++threshold) {
        Id id1 = 0;
        Id id2 = 0;
        graph_lite_graph* g = make_two_nodes(&id1, &id2);
        ASSERT_NE(g, nullptr);
        Id edge = 0;
        ASSERT_EQ(graph_lite_edge_add(g, id1, id2, 7, &edge), GRAPH_LITE_OK);

        graph_lite_status st = GRAPH_LITE_OK;
        {
            AllocProbe::Scope refusal(threshold);
            st = graph_lite_node_remove(g, id1);
        }

        if (st == GRAPH_LITE_OK) {
            succeeded = true;
            uint64_t nodes = 0;
            EXPECT_EQ(graph_lite_graph_node_count(g, &nodes), GRAPH_LITE_OK);
            EXPECT_EQ(nodes, 1u);
            uint64_t type = 0;
            EXPECT_EQ(graph_lite_node_type(g, id2, &type), GRAPH_LITE_OK);
            EXPECT_EQ(type, 20u);
        } else {
            EXPECT_EQ(st, GRAPH_LITE_ERR_OUT_OF_MEMORY) << "threshold " << threshold;
            uint64_t edges = 99;
            EXPECT_EQ(graph_lite_graph_edge_count(g, &edges), GRAPH_LITE_OK);
            EXPECT_EQ(edges, 0u);
            uint64_t type = 0;
            EXPECT_EQ(graph_lite_edge_type(g, edge, &type), GRAPH_LITE_ERR_NOT_FOUND);
            uint64_t nodes = 99;
            EXPECT_EQ(graph_lite_graph_node_count(g, &nodes), GRAPH_LITE_OK);
            EXPECT_EQ(nodes, 2u);
            EXPECT_EQ(graph_lite_node_type(g, id1, &type), GRAPH_LITE_OK);
            EXPECT_EQ(type, 10u);
            EXPECT_EQ(graph_lite_node_type(g, id2, &type), GRAPH_LITE_OK);
            EXPECT_EQ(type, 20u);
        }
        graph_lite_graph_destroy(g);
    }
    EXPECT_TRUE(succeeded) << "no threshold let node_remove complete its allocations";
}

// A refused removal of a node holding edges in both directions is the shape
// where a half-committed run release bites: the still-live node would keep a
// run already listed as free, a later edge could claim those cells, and the
// next inserts would overwrite each other's adjacency. Every refusal must
// leave the survivors exact, keep accepting new edges, and let a retry finish
// the removal.
TEST(AllocFailure, NodeRemoveRefusalWithBothDirectionsKeepsGraphExact) {
    bool succeeded = false;
    for (int threshold = 1; threshold <= kMaxThreshold && !succeeded; ++threshold) {
        graph_lite_graph* g = graph_lite_graph_create();
        ASSERT_NE(g, nullptr);
        Id a = 0;
        Id b = 0;
        Id c = 0;
        ASSERT_EQ(graph_lite_node_add(g, 10, &a), GRAPH_LITE_OK);
        ASSERT_EQ(graph_lite_node_add(g, 20, &b), GRAPH_LITE_OK);
        ASSERT_EQ(graph_lite_node_add(g, 30, &c), GRAPH_LITE_OK);
        Id dead[4] = {0, 0, 0, 0}; // b's incident edges, drained by its removal
        ASSERT_EQ(graph_lite_edge_add(g, a, b, 1, &dead[0]), GRAPH_LITE_OK);
        ASSERT_EQ(graph_lite_edge_add(g, a, b, 2, &dead[1]), GRAPH_LITE_OK);
        ASSERT_EQ(graph_lite_edge_add(g, b, c, 3, &dead[2]), GRAPH_LITE_OK);
        ASSERT_EQ(graph_lite_edge_add(g, b, c, 4, &dead[3]), GRAPH_LITE_OK);
        Id kept = 0;
        ASSERT_EQ(graph_lite_edge_add(g, a, c, 5, &kept), GRAPH_LITE_OK);

        graph_lite_status st = GRAPH_LITE_OK;
        {
            AllocProbe::Scope refusal(threshold);
            st = graph_lite_node_remove(g, b);
        }

        if (st == GRAPH_LITE_OK) {
            succeeded = true;
            uint64_t nodes = 99;
            EXPECT_EQ(graph_lite_graph_node_count(g, &nodes), GRAPH_LITE_OK);
            EXPECT_EQ(nodes, 2u);
            uint64_t type = 99;
            EXPECT_EQ(graph_lite_node_type(g, b, &type), GRAPH_LITE_ERR_NOT_FOUND);
            expect_node_answers(g, a, 10, 1, 0, threshold);
            expect_node_answers(g, c, 30, 0, 1, threshold);
            expect_neighbors(g, a, true, {c}, threshold);
            expect_neighbors(g, c, false, {a}, threshold);
        } else {
            EXPECT_EQ(st, GRAPH_LITE_ERR_OUT_OF_MEMORY) << "threshold " << threshold;
            // The drain is allocation-free and stays committed; b itself is
            // still live but answers for none of its edges.
            uint64_t edges = 99;
            EXPECT_EQ(graph_lite_graph_edge_count(g, &edges), GRAPH_LITE_OK);
            EXPECT_EQ(edges, 1u);
            uint64_t type = 99;
            for (const Id id : dead)
                EXPECT_EQ(graph_lite_edge_type(g, id, &type), GRAPH_LITE_ERR_NOT_FOUND)
                    << "threshold " << threshold;
            expect_node_answers(g, b, 20, 0, 0, threshold);
            expect_node_answers(g, a, 10, 1, 0, threshold);
            expect_node_answers(g, c, 30, 0, 1, threshold);
            expect_neighbors(g, a, true, {c}, threshold);
            expect_neighbors(g, c, false, {a}, threshold);
            // The survivors keep accepting edges — straight through whatever
            // the failed attempt released.
            Id fresh = 0;
            ASSERT_EQ(graph_lite_edge_add(g, c, a, 6, &fresh), GRAPH_LITE_OK)
                << "threshold " << threshold;
            expect_node_answers(g, a, 10, 1, 1, threshold);
            expect_node_answers(g, c, 30, 1, 1, threshold);
            expect_neighbors(g, a, false, {c}, threshold);
            expect_neighbors(g, c, true, {a}, threshold);
            // The retry finishes the removal; the survivors keep answering by
            // id across the record shuffle it performs.
            ASSERT_EQ(graph_lite_node_remove(g, b), GRAPH_LITE_OK) << "threshold " << threshold;
            uint64_t nodes = 99;
            EXPECT_EQ(graph_lite_graph_node_count(g, &nodes), GRAPH_LITE_OK);
            EXPECT_EQ(nodes, 2u);
            EXPECT_EQ(graph_lite_node_type(g, b, &type), GRAPH_LITE_ERR_NOT_FOUND);
            expect_node_answers(g, a, 10, 1, 1, threshold);
            expect_node_answers(g, c, 30, 1, 1, threshold);
            expect_neighbors(g, a, false, {c}, threshold);
            expect_neighbors(g, c, false, {a}, threshold);
        }
        graph_lite_graph_destroy(g);
    }
    EXPECT_TRUE(succeeded) << "no threshold let node_remove complete its allocations";
}

TEST(AllocFailure, CreateReturnsNullWhenAllocationFails) {
    bool saw_null = false;
    for (int threshold = 1; threshold <= 8; ++threshold) {
        graph_lite_graph* g = nullptr;
        {
            AllocProbe::Scope refusal(threshold);
            g = graph_lite_graph_create();
        }
        if (g == nullptr) {
            saw_null = true;
            continue;
        }
        // Whatever got through must still be a working empty store.
        uint64_t nodes = 99;
        EXPECT_EQ(graph_lite_graph_node_count(g, &nodes), GRAPH_LITE_OK);
        EXPECT_EQ(nodes, 0u);
        graph_lite_graph_destroy(g);
    }
    EXPECT_TRUE(saw_null) << "refusing the first allocation must yield NULL, not an exception";
}
