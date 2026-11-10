// Store correctness in the generic node/edge vocabulary only: a shadow model
// (map/multiset adjacency) tracks a fixed-seed churn, compared after EVERY
// operation so a divergence names its cause. Guard: scripts/lint/graph-lite-vocabulary.ts
#include "graph-lite.h"

#include "graph-lite.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <utility>
#include <vector>

namespace {

using Id = graph_lite_id;

Id expect_node_add(graph_lite_graph* g, graph_lite_tag tag) {
    Id id = 0;
    EXPECT_EQ(graph_lite_add_node(g, tag, &id), GRAPH_LITE_OK);
    return id;
}

Id expect_edge_add(graph_lite_graph* g, Id from, Id to, graph_lite_tag tag) {
    Id id = 0;
    EXPECT_EQ(graph_lite_add_edge(g, from, to, tag, &id), GRAPH_LITE_OK);
    return id;
}

struct ShadowEdge {
    Id from;
    Id to;
    uint64_t type;
};

// What a naive correct implementation would hold. Adjacency is a MULTIset: a
// parallel edge pair contributes one entry per edge, matching how the store
// counts degrees, and pair_ids carries the edge ids per node pair.
struct Shadow {
    std::map<Id, uint64_t> node_types;
    std::map<Id, ShadowEdge> edges;
    std::map<std::pair<Id, Id>, std::vector<Id>> pair_ids;
    std::map<Id, std::multiset<Id>> out;
    std::map<Id, std::multiset<Id>> in;

    void add_node(Id id, uint64_t type) {
        node_types[id] = type;
        out[id];
        in[id];
    }

    void add_edge(Id id, Id from, Id to, uint64_t type) {
        edges[id] = {from, to, type};
        pair_ids[{from, to}].push_back(id);
        out[from].insert(to);
        in[to].insert(from);
    }

    void remove_edge(Id id) {
        const ShadowEdge e = edges.at(id);
        std::vector<Id>& ids = pair_ids.at({e.from, e.to});
        ids.erase(std::find(ids.begin(), ids.end(), id));
        if (ids.empty())
            pair_ids.erase({e.from, e.to});
        out[e.from].erase(out[e.from].find(e.to)); // one occurrence, not the class
        in[e.to].erase(in[e.to].find(e.from));
        edges.erase(id);
    }

    void remove_node(Id id) {
        // Copies: multisets mutate while ids drain; the set gives one pass per pair.
        const std::set<Id> outs(out[id].begin(), out[id].end());
        for (Id to : outs) {
            for (Id eid : pair_ids.at({id, to})) {
                edges.erase(eid);
                in[to].erase(id); // every parallel copy of the pair dies too
            }
            pair_ids.erase({id, to});
            out[id].erase(to);
        }
        const std::set<Id> ins(in[id].begin(), in[id].end());
        for (Id from : ins) {
            if (from == id)
                continue; // a self-loop was fully drained on the out pass above
            for (Id eid : pair_ids.at({from, id})) {
                edges.erase(eid);
                out[from].erase(id);
            }
            pair_ids.erase({from, id});
            in[id].erase(from);
        }
        out.erase(id);
        in.erase(id);
        node_types.erase(id);
    }
};

::testing::AssertionResult neighbors_match(const graph_lite_graph* g, Id id,
                                           const std::multiset<Id>& want, bool forward) {
    uint64_t deg = 0;
    const graph_lite_status d =
        forward ? graph_lite_node_out_degree(g, id, &deg) : graph_lite_node_in_degree(g, id, &deg);
    if (d != GRAPH_LITE_OK || deg != want.size())
        return ::testing::AssertionFailure() << "degree mismatch for id " << id;

    std::vector<Id> got(deg, 0);
    uint64_t count = 0;
    const graph_lite_status q = forward
                                    ? graph_lite_node_out_neighbors(g, id, got.data(), deg, &count)
                                    : graph_lite_node_in_neighbors(g, id, got.data(), deg, &count);
    if (q != GRAPH_LITE_OK || count != deg)
        return ::testing::AssertionFailure() << "neighbor query mismatch for id " << id;

    std::sort(got.begin(), got.end());
    if (!std::equal(got.begin(), got.end(), want.begin()))
        return ::testing::AssertionFailure() << "neighbor sets differ for id " << id;
    return ::testing::AssertionSuccess();
}

// The whole-store invariant check: counts, degrees, both neighbor directions,
// per-edge liveness, and edge count == summed degree in each direction.
::testing::AssertionResult matches_store(const graph_lite_graph* g, const Shadow& s) {
    if (graph_lite_node_count(g) != s.node_types.size())
        return ::testing::AssertionFailure() << "node count mismatch";
    if (graph_lite_edge_count(g) != s.edges.size())
        return ::testing::AssertionFailure() << "edge count mismatch";

    uint64_t out_sum = 0;
    uint64_t in_sum = 0;
    for (const auto& entry : s.node_types) {
        const Id id = entry.first;
        graph_lite_tag got_tag = 0;
        uint64_t out_deg = 0;
        uint64_t in_deg = 0;
        if (graph_lite_node_tag(g, id, &got_tag) != GRAPH_LITE_OK || got_tag != entry.second)
            return ::testing::AssertionFailure() << "type mismatch for id " << id;
        if (graph_lite_node_out_degree(g, id, &out_deg) != GRAPH_LITE_OK ||
            out_deg != s.out.at(id).size())
            return ::testing::AssertionFailure() << "out degree mismatch for id " << id;
        if (graph_lite_node_in_degree(g, id, &in_deg) != GRAPH_LITE_OK ||
            in_deg != s.in.at(id).size())
            return ::testing::AssertionFailure() << "in degree mismatch for id " << id;
        out_sum += out_deg;
        in_sum += in_deg;
        ::testing::AssertionResult res = neighbors_match(g, id, s.out.at(id), true);
        if (!res)
            return res;
        res = neighbors_match(g, id, s.in.at(id), false);
        if (!res)
            return res;
    }
    if (out_sum != s.edges.size() || in_sum != s.edges.size())
        return ::testing::AssertionFailure()
               << "edge count " << s.edges.size() << " != summed degrees";
    for (const auto& entry : s.edges) {
        graph_lite_tag got_tag = 0;
        if (graph_lite_edge_info(g, entry.first, nullptr, nullptr, &got_tag) != GRAPH_LITE_OK ||
            got_tag != entry.second.type)
            return ::testing::AssertionFailure() << "edge type mismatch for id " << entry.first;
    }
    return ::testing::AssertionSuccess();
}

class StoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        const graph_lite_status created = graph_lite_graph_create(&graph_);
        ASSERT_EQ(created, GRAPH_LITE_OK);
        ASSERT_NE(graph_, nullptr);
    }

    void TearDown() override { graph_lite_graph_destroy(graph_); }

    graph_lite_graph* graph_ = nullptr;
};

TEST_F(StoreTest, EmptyStoreAnswers) {
    EXPECT_EQ(graph_lite_node_count(graph_), 0u);
    EXPECT_EQ(graph_lite_edge_count(graph_), 0u);
    graph_lite_graph_destroy(nullptr); // must be a no-op
}

TEST_F(StoreTest, NodeLifecycleAndMonotonicIds) {
    const Id a = expect_node_add(graph_, 10);
    const Id b = expect_node_add(graph_, 20);
    const Id c = expect_node_add(graph_, 30);
    EXPECT_GT(b, a);
    EXPECT_GT(c, b);

    graph_lite_tag tag = 0;
    ASSERT_EQ(graph_lite_node_tag(graph_, b, &tag), GRAPH_LITE_OK);
    EXPECT_EQ(tag, 20u);

    ASSERT_EQ(graph_lite_remove_node(graph_, b), GRAPH_LITE_OK);
    EXPECT_EQ(graph_lite_node_tag(graph_, b, &tag), GRAPH_LITE_ERR_NOT_FOUND);
    const Id fresh = expect_node_add(graph_, 40);
    EXPECT_GT(fresh, c); // the freed id never comes back

    // A missing endpoint fails the edge and changes nothing.
    Id edge = 0;
    EXPECT_EQ(graph_lite_add_edge(graph_, a, b, 1, &edge), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(edge, 0u);
}

TEST_F(StoreTest, DegreesAndNeighborsBothDirections) {
    const Id a = expect_node_add(graph_, 1);
    const Id b = expect_node_add(graph_, 2);
    const Id c = expect_node_add(graph_, 3);
    const Id e1 = expect_edge_add(graph_, a, b, 10);
    (void)expect_edge_add(graph_, a, c, 11);
    (void)expect_edge_add(graph_, b, c, 12);

    uint64_t deg = 0;
    ASSERT_EQ(graph_lite_node_out_degree(graph_, a, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 2u);
    ASSERT_EQ(graph_lite_node_in_degree(graph_, a, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 0u);
    ASSERT_EQ(graph_lite_node_in_degree(graph_, c, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 2u);

    // A freshly built run appends, so order == insertion order; this pins OUR
    // forward layout, while every other test compares sorted sets.
    Id buf[4] = {0};
    uint64_t count = 0;
    ASSERT_EQ(graph_lite_node_out_neighbors(graph_, a, buf, 4, &count), GRAPH_LITE_OK);
    ASSERT_EQ(count, 2u);
    EXPECT_EQ(buf[0], b);
    EXPECT_EQ(buf[1], c);

    // Parallel edges are legal and each direction counts them.
    (void)expect_edge_add(graph_, a, b, 13);
    ASSERT_EQ(graph_lite_node_out_degree(graph_, a, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 3u);
    ASSERT_EQ(graph_lite_node_in_degree(graph_, b, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 2u);

    // A self-loop counts once in each direction; removing it clears both.
    const Id loop = expect_edge_add(graph_, a, a, 14);
    ASSERT_EQ(graph_lite_node_out_degree(graph_, a, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 4u);
    ASSERT_EQ(graph_lite_remove_edge(graph_, loop), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_node_in_degree(graph_, a, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 0u);

    ASSERT_EQ(graph_lite_remove_edge(graph_, e1), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_node_out_degree(graph_, a, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 2u); // self-loop and e1 are gone
    ASSERT_EQ(graph_lite_node_in_degree(graph_, b, &deg), GRAPH_LITE_OK);
    EXPECT_EQ(deg, 1u);
    EXPECT_EQ(graph_lite_edge_info(graph_, e1, nullptr, nullptr, nullptr),
              GRAPH_LITE_ERR_NOT_FOUND);
}

TEST_F(StoreTest, BufferTooSmallLeavesBufferUntouched) {
    const Id a = expect_node_add(graph_, 1);
    const Id t1 = expect_node_add(graph_, 2);
    const Id t2 = expect_node_add(graph_, 3);
    const Id t3 = expect_node_add(graph_, 4);
    const Id sink = expect_node_add(graph_, 5);
    (void)expect_edge_add(graph_, a, t1, 1);
    (void)expect_edge_add(graph_, a, t2, 2);
    (void)expect_edge_add(graph_, a, t3, 3);
    (void)expect_edge_add(graph_, sink, a, 4);
    (void)expect_edge_add(graph_, sink, a, 5);

    Id raw[8];
    std::memset(raw, 0xAB, sizeof raw);
    uint64_t count = 0;
    ASSERT_EQ(graph_lite_node_out_neighbors(graph_, a, raw, 2, &count),
              GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    ASSERT_EQ(count, 3u); // the required count, not the capacity
    Id untouched[8];
    std::memset(untouched, 0xAB, sizeof untouched);
    EXPECT_EQ(std::memcmp(raw, untouched, sizeof raw), 0);

    // Exactly enough room succeeds.
    ASSERT_EQ(graph_lite_node_out_neighbors(graph_, a, raw, 3, &count), GRAPH_LITE_OK);
    ASSERT_EQ(count, 3u);
    std::vector<Id> got(raw, raw + 3);
    std::sort(got.begin(), got.end());
    EXPECT_EQ(got, (std::vector<Id>{t1, t2, t3}));

    // NULL buffer: capacity 0 stays the "how many?" probe; with room, an error.
    ASSERT_EQ(graph_lite_node_out_neighbors(graph_, a, nullptr, 0, &count),
              GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(graph_lite_node_out_neighbors(graph_, a, nullptr, 8, &count),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    const Id empty = expect_node_add(graph_, 9);
    ASSERT_EQ(graph_lite_node_out_neighbors(graph_, empty, nullptr, 0, &count), GRAPH_LITE_OK);
    EXPECT_EQ(count, 0u);
}

TEST_F(StoreTest, NullAndUnknownArgumentsGetDistinctCodes) {
    uint64_t v = 0;
    Id id = 0;

    EXPECT_EQ(graph_lite_graph_memory_usage(nullptr, &v), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_add_node(nullptr, 1, &id), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_remove_node(nullptr, 1), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_node_tag(nullptr, 1, nullptr), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_node_out_degree(nullptr, 1, &v), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_node_out_neighbors(nullptr, 1, nullptr, 0, &v),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_add_edge(nullptr, 1, 1, 1, &id), GRAPH_LITE_ERR_INVALID_ARGUMENT);

    // Valid handle, missing out-parameter — still INVALID_ARGUMENT, still first.
    EXPECT_EQ(graph_lite_add_node(graph_, 1, nullptr), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_node_tag(graph_, 1, nullptr), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_add_edge(graph_, 1, 1, 1, nullptr), GRAPH_LITE_ERR_INVALID_ARGUMENT);

    // Valid call shape, unknown identity — NOT_FOUND, a different decision.
    const Id unknown = 1000000;
    EXPECT_EQ(graph_lite_node_tag(graph_, unknown, &v), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_node_out_degree(graph_, unknown, &v), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_node_out_neighbors(graph_, unknown, nullptr, 0, &v),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_remove_node(graph_, unknown), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_edge_info(graph_, unknown, nullptr, nullptr, nullptr),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_remove_edge(graph_, unknown), GRAPH_LITE_ERR_NOT_FOUND);
    const Id a = expect_node_add(graph_, 1);
    EXPECT_EQ(graph_lite_add_edge(graph_, unknown, a, 1, &id), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_add_edge(graph_, a, unknown, 1, &id), GRAPH_LITE_ERR_NOT_FOUND);

    // Distinct reasons stay distinct values — the numbers are ABI.
    EXPECT_NE(GRAPH_LITE_OK, GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_NE(GRAPH_LITE_OK, GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_NE(GRAPH_LITE_OK, GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    EXPECT_NE(GRAPH_LITE_ERR_INVALID_ARGUMENT, GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_NE(GRAPH_LITE_ERR_INVALID_ARGUMENT, GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    EXPECT_NE(GRAPH_LITE_ERR_NOT_FOUND, GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
}

TEST_F(StoreTest, NodeRemovalCascadesBothDirections) {
    const Id hub = expect_node_add(graph_, 1);
    const Id t1 = expect_node_add(graph_, 2);
    const Id t2 = expect_node_add(graph_, 3);
    const Id s1 = expect_node_add(graph_, 4);
    const Id s2 = expect_node_add(graph_, 5);
    const Id out1 = expect_edge_add(graph_, hub, t1, 1);
    const Id out2 = expect_edge_add(graph_, hub, t2, 2);
    const Id self = expect_edge_add(graph_, hub, hub, 3);
    const Id in1 = expect_edge_add(graph_, s1, hub, 4);
    const Id in2 = expect_edge_add(graph_, s2, hub, 5);
    const Id hub_edges[] = {out1, out2, self, in1, in2};

    Shadow shadow;
    const Id nodes[] = {hub, t1, t2, s1, s2};
    const uint64_t types[] = {1, 2, 3, 4, 5};
    for (size_t i = 0; i < 5; ++i)
        shadow.add_node(nodes[i], types[i]);
    const Id ends[][2] = {{hub, t1}, {hub, t2}, {hub, hub}, {s1, hub}, {s2, hub}};
    const uint64_t etypes[] = {1, 2, 3, 4, 5};
    for (size_t i = 0; i < 5; ++i)
        shadow.add_edge(hub_edges[i], ends[i][0], ends[i][1], etypes[i]);

    shadow.remove_node(hub);
    ASSERT_EQ(graph_lite_remove_node(graph_, hub), GRAPH_LITE_OK);
    ASSERT_TRUE(matches_store(graph_, shadow));

    // Every incident edge id is unreadable, from either side, forever.
    for (const Id eid : hub_edges) {
        EXPECT_EQ(graph_lite_edge_info(graph_, eid, nullptr, nullptr, nullptr),
                  GRAPH_LITE_ERR_NOT_FOUND)
            << "edge id " << eid;
    }
}

TEST_F(StoreTest, IdsAreNeverReused) {
    Id max_node = 0;
    Id max_edge = 0;
    Id survivor = 0;
    for (int round = 0; round < 10; ++round) {
        const Id n1 = expect_node_add(graph_, 1);
        const Id n2 = expect_node_add(graph_, 2);
        const Id n3 = expect_node_add(graph_, 3);
        ASSERT_GT(n1, max_node);
        ASSERT_GT(n2, n1);
        ASSERT_GT(n3, n2);
        max_node = n3;
        survivor = n3;
        const Id e1 = expect_edge_add(graph_, n1, n2, 1);
        const Id e2 = expect_edge_add(graph_, n2, n3, 2);
        ASSERT_GT(e1, max_edge);
        ASSERT_GT(e2, e1);
        max_edge = e2;
        ASSERT_EQ(graph_lite_remove_node(graph_, n1), GRAPH_LITE_OK); // cascades e1
        ASSERT_EQ(graph_lite_remove_edge(graph_, e2), GRAPH_LITE_OK);
    }
    const Id fresh = expect_node_add(graph_, 9);
    ASSERT_GT(fresh, max_node);
    const Id fresh_edge = expect_edge_add(graph_, fresh, survivor, 9);
    ASSERT_GT(fresh_edge, max_edge);

    EXPECT_EQ(graph_lite_node_count(graph_), 21u);
    EXPECT_EQ(graph_lite_edge_count(graph_), 1u);
}

TEST_F(StoreTest, MutationChurnMatchesShadowModel) {
    std::mt19937_64 rng(42); // fixed seed: failures reproduce
    Shadow shadow;
    std::vector<Id> live_nodes;
    std::vector<Id> live_edges;
    Id max_node_id = 0;
    Id max_edge_id = 0;

    for (int op = 0; op < 1500; ++op) {
        const unsigned roll = static_cast<unsigned>(rng() % 100);
        if (roll < 40 || live_nodes.empty()) {
            const uint64_t type = rng() % 16;
            const Id id = expect_node_add(graph_, type);
            ASSERT_GT(id, max_node_id); // node ids: one monotonic space
            max_node_id = id;
            shadow.add_node(id, type);
            live_nodes.push_back(id);
        } else if (roll < 62) {
            const Id from = live_nodes[static_cast<size_t>(rng() % live_nodes.size())];
            const Id to = live_nodes[static_cast<size_t>(rng() % live_nodes.size())];
            const uint64_t type = rng() % 8;
            const Id id = expect_edge_add(graph_, from, to, type);
            ASSERT_GT(id, max_edge_id); // edge ids: a separate monotonic space
            max_edge_id = id;
            shadow.add_edge(id, from, to, type);
            live_edges.push_back(id);
        } else if (roll < 84 && !live_edges.empty()) {
            const size_t at = static_cast<size_t>(rng() % live_edges.size());
            const Id id = live_edges[at];
            live_edges[at] = live_edges.back();
            live_edges.pop_back();
            ASSERT_EQ(graph_lite_remove_edge(graph_, id), GRAPH_LITE_OK);
            shadow.remove_edge(id);
        } else {
            const size_t at = static_cast<size_t>(rng() % live_nodes.size());
            const Id id = live_nodes[at];
            live_nodes[at] = live_nodes.back();
            live_nodes.pop_back();
            // The cascade kills every incident edge; purge the dead ids from
            // the live-edge pool so a later pick cannot name a dead edge.
            std::vector<Id> dying;
            for (const auto& entry : shadow.pair_ids)
                if (entry.first.first == id || entry.first.second == id)
                    dying.insert(dying.end(), entry.second.begin(), entry.second.end());
            for (Id eid : dying) {
                const auto it = std::find(live_edges.begin(), live_edges.end(), eid);
                ASSERT_NE(it, live_edges.end());
                live_edges.erase(it);
            }
            ASSERT_EQ(graph_lite_remove_node(graph_, id), GRAPH_LITE_OK);
            shadow.remove_node(id);
        }
        ASSERT_TRUE(matches_store(graph_, shadow)) << "diverged after operation " << op;
    }
}

TEST(HppGraphWrapper, RoundTripAndMove) {
    graph_lite::Graph g;
    const Id a = g.add_node(1);
    const Id b = g.add_node(2);
    const Id e = g.add_edge(a, b, 3);
    EXPECT_EQ(g.node_count(), 2u);
    EXPECT_EQ(g.edge_count(), 1u);
    EXPECT_EQ(g.node_tag(a), 1u);
    EXPECT_EQ(g.edge_tag(e), 3u);
    EXPECT_EQ(g.out_degree(a), 1u);
    EXPECT_GT(g.memory_usage(), 0u);

    const std::vector<Id> out = g.out_neighbors(a);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], b);
    EXPECT_EQ(g.in_neighbors(b), std::vector<Id>{a});

    graph_lite::Graph moved = std::move(g);
    EXPECT_EQ(moved.node_count(), 2u);
    moved.remove_edge(e);
    moved.remove_node(a);
    EXPECT_EQ(moved.edge_count(), 0u);
    EXPECT_EQ(moved.node_count(), 1u);

    graph_lite::Graph target;
    target = std::move(moved); // destroys target's previous handle, not moved's
    EXPECT_EQ(target.node_count(), 1u);
    EXPECT_THROW(target.remove_node(999999), graph_lite::Error);
}

} // namespace
