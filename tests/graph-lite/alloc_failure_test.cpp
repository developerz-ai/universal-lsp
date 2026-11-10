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

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <vector>

namespace {

// Process-wide refusal probe. The store is single-writer and this binary is
// single-threaded, so plain statics are safe. Armed with N, the N-th allocation
// attempt is refused; disarmed (-1), everything passes through.
class AllocProbe {
public:
    // RAII: the armed window holds only the C call under test, so gtest's own
    // allocations — failure-message building included — never see a refusal,
    // and an escaping exception (the regression this suite exists to catch)
    // still leaves the probe disarmed for the rest of the binary.
    class Scope {
    public:
        explicit Scope(int fail_on) { AllocProbe::arm(fail_on); }

        ~Scope() { AllocProbe::disarm(); }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };

    static void arm(int fail_on) { remaining_ = fail_on; }

    static void disarm() { remaining_ = -1; }

    // Consume one attempt; true means this one must fail. The N-th attempt of
    // an armed window is the refused one, and the window stays refused after
    // that — a budget, not a one-shot.
    static bool refuse() {
        if (remaining_ < 0)
            return false;
        if (remaining_ == 1)
            return true;
        --remaining_;
        return false;
    }

private:
    inline static int remaining_ = -1;
};

void* raw_alloc(std::size_t size) {
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

} // namespace

// The global allocation functions are replaced so a refusal lands exactly where
// the store allocates. The throwing forms throw — how vector and unordered_map
// report failure; the nothrow forms return null, the contract
// `new (std::nothrow)` is written against and the path create()'s raw
// allocation takes.
void* operator new(std::size_t size) {
    if (AllocProbe::refuse())
        throw std::bad_alloc();
    return raw_alloc(size);
}

void* operator new[](std::size_t size) {
    if (AllocProbe::refuse())
        throw std::bad_alloc();
    return raw_alloc(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (AllocProbe::refuse())
        return nullptr;
    return std::malloc(size == 0 ? 1 : size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    if (AllocProbe::refuse())
        return nullptr;
    return std::malloc(size == 0 ? 1 : size);
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}

void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}

namespace {

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
            if (threshold == 1) {
                // The first refusal lands before the store changes anything, so
                // the counts must be exactly what they were.
                uint64_t edges = 99;
                EXPECT_EQ(graph_lite_graph_edge_count(g, &edges), GRAPH_LITE_OK);
                EXPECT_EQ(edges, 0u);
            }
        }
        graph_lite_graph_destroy(g);
    }
    EXPECT_TRUE(succeeded) << "no threshold let edge_add complete its allocations";
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
