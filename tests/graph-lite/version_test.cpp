// Nothing under tests/graph-lite/ may name a caller's domain concept. If a test
// here needs borrowed vocabulary to describe itself, the API leaked and the
// extraction just got more expensive. Guard: scripts/lint/graph-lite-vocabulary.ts
#include "graph-lite.h"

#include "graph-lite.hpp"

#include <gtest/gtest.h>

#include <type_traits>

extern "C" int graph_lite_c_abi_smoke(void);

TEST(Version, LinkedLibraryMatchesHeader) {
    const auto v = graph_lite::version();
    EXPECT_EQ(v.major, GRAPH_LITE_VERSION_MAJOR);
    EXPECT_EQ(v.minor, GRAPH_LITE_VERSION_MINOR);
    EXPECT_EQ(v.patch, GRAPH_LITE_VERSION_PATCH);
}

TEST(Version, StringMatchesComponents) {
    EXPECT_STREQ(graph_lite::version_string(), GRAPH_LITE_VERSION_STRING);
}

TEST(CAbi, HeaderCompilesAndLinksAsC) {
    // The assertions live in the .c file; a non-zero return names which one
    // failed. This test exists so the C translation unit is actually linked —
    // a file that compiles but is never linked proves nothing about the ABI.
    EXPECT_EQ(graph_lite_c_abi_smoke(), 0);
}

TEST(CAbi, StatusStringIsNeverNull) {
    EXPECT_STREQ(graph_lite_status_string(GRAPH_LITE_OK), "ok");
    EXPECT_NE(graph_lite_status_string(static_cast<graph_lite_status>(9999)), nullptr);
}

TEST(CAbi, ErrorCarriesTheCodeNotJustAMessage) {
    // A binding that only gets a message cannot branch on the reason.
    try {
        graph_lite::check(GRAPH_LITE_ERR_NOT_FOUND);
        FAIL() << "check() must throw on a non-OK status";
    } catch (const graph_lite::Error& e) {
        EXPECT_EQ(e.status(), GRAPH_LITE_ERR_NOT_FOUND);
    }
}

/* --- the v0.1.0 surface: RAII wrappers, appended status codes ------------- */

static_assert(std::is_move_constructible_v<graph_lite::Store>, "Store is move-only");
static_assert(!std::is_copy_constructible_v<graph_lite::Store>, "Store is move-only");
static_assert(std::is_move_constructible_v<graph_lite::Cursor>, "Cursor is move-only");
static_assert(!std::is_copy_constructible_v<graph_lite::Cursor>, "Cursor is move-only");

TEST(StoreRaii, WrapsTheWholeStoreSurface) {
    graph_lite::Store store;
    const auto a = store.add_node(1);
    const auto b = store.add_node(2);
    const auto edge = store.add_edge(a, b, 3);
    EXPECT_EQ(store.node_count(), 2u);
    EXPECT_EQ(store.edge_count(), 1u);
    EXPECT_EQ(store.node_type(b), 2u);
    EXPECT_EQ(store.edge_type(edge), 3u);
    const auto endpoints = store.endpoints(edge);
    EXPECT_EQ(endpoints.from, a);
    EXPECT_EQ(endpoints.to, b);
    store.remove_edge(edge);
    EXPECT_EQ(store.edge_count(), 0u);
    store.remove_node(a);
    EXPECT_EQ(store.node_count(), 1u);
}

TEST(StoreRaii, MoveLeavesTheSourceEmptyAndDestroysExactlyOnce) {
    graph_lite::Store source;
    graph_lite::Store target(std::move(source));
    EXPECT_EQ(target.add_node(1), 1u);
    target = graph_lite::Store(); // the old handle dies inside the assignment
    EXPECT_EQ(target.node_count(), 0u);
}

TEST(CursorRaii, WalksTheStoreAndReportsItsShape) {
    graph_lite::Store store;
    const auto a = store.add_node(1);
    const auto b = store.add_node(1);
    store.add_edge(a, b, 1);
    graph_lite::Cursor cursor(store, a, GRAPH_LITE_BREADTH_FIRST, GRAPH_LITE_OUTGOING);
    graph_lite_node_id id = 0;
    ASSERT_TRUE(cursor.next(id));
    EXPECT_EQ(id, a);
    ASSERT_TRUE(cursor.next(id));
    EXPECT_EQ(id, b);
    EXPECT_EQ(cursor.depth(), 1u);
    EXPECT_FALSE(cursor.next(id));
    EXPECT_FALSE(cursor.truncated());
}

TEST(Check, SurfacesTheCodeBehindTheFailure) {
    graph_lite::Store store;
    try {
        store.node_type(42);
        FAIL() << "an unknown id must throw";
    } catch (const graph_lite::Error& e) {
        EXPECT_EQ(e.status(), GRAPH_LITE_ERR_NOT_FOUND);
    }
    // A malformed call throws the same typed error, with its own code: a
    // moved-from store holds no handle, so the C entry point answers
    // INVALID_ARGUMENT and check() surfaces it as this typed exception.
    graph_lite::Store moved_away(std::move(store));
    try {
        store.add_node(1);
        FAIL() << "a moved-from store has no handle";
    } catch (const graph_lite::Error& e) {
        EXPECT_EQ(e.status(), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    }
}

TEST(CAbi, StatusStringsCoverTheAppendedCodes) {
    EXPECT_STREQ(graph_lite_status_string(GRAPH_LITE_DONE), "done");
    EXPECT_STREQ(graph_lite_status_string(GRAPH_LITE_ERR_UNREACHABLE), "unreachable");
    // Out of range stays non-NULL, higher than every known code included.
    EXPECT_STREQ(graph_lite_status_string(static_cast<graph_lite_status>(1000000)),
                 "unknown status");
}
