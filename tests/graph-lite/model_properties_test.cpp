// Properties of the typed node/edge store, through the C header only. Ids, tags,
// edges and the removal cascade are in model_test.cpp. Nothing under
// tests/graph-lite/ may name a caller's domain concept.
// Guard: scripts/lint/graph-lite-vocabulary.ts
#include "graph-lite.h"

#include "model_test_support.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>

namespace {

bool fail_allocations = false;

template <typename Fn>
graph_lite_status call_with_allocations_disabled(Fn&& fn, bool* threw) {
    fail_allocations = true;
    try {
        const graph_lite_status status = fn();
        fail_allocations = false;
        return status;
    } catch (const std::bad_alloc&) {
        fail_allocations = false;
        *threw = true;
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    }
}

} // namespace

void* operator new(std::size_t size) {
    if (fail_allocations)
        throw std::bad_alloc();
    if (void* allocation = std::malloc(size))
        return allocation;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    return operator new(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return operator new(size);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return operator new(size, std::nothrow);
}

void operator delete(void* allocation) noexcept {
    std::free(allocation);
}

void operator delete(void* allocation, std::size_t) noexcept {
    std::free(allocation);
}

void operator delete[](void* allocation) noexcept {
    std::free(allocation);
}

void operator delete[](void* allocation, std::size_t) noexcept {
    std::free(allocation);
}

// --- properties ------------------------------------------------------------

TEST(Model, NodeIntPropertyRoundTripsAndRemoves) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, n, "count", INT64_MIN), GRAPH_LITE_OK);

    graph_lite_value_kind kind = GRAPH_LITE_VALUE_BOOL;
    ASSERT_EQ(graph_lite_property_kind(g.get(), GRAPH_LITE_NODE, n, "count", &kind), GRAPH_LITE_OK);
    EXPECT_EQ(kind, GRAPH_LITE_VALUE_INT);

    int64_t got = 0;
    ASSERT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, "count", &got), GRAPH_LITE_OK);
    EXPECT_EQ(got, INT64_MIN);

    ASSERT_EQ(graph_lite_remove_property(g.get(), GRAPH_LITE_NODE, n, "count"), GRAPH_LITE_OK);
    EXPECT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, "count", &got),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_property_kind(g.get(), GRAPH_LITE_NODE, n, "count", &kind),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_remove_property(g.get(), GRAPH_LITE_NODE, n, "count"),
              GRAPH_LITE_ERR_NOT_FOUND);
}

TEST(Model, EdgeDoublePropertyRoundTrips) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto e = add_edge(g.get(), a, a);
    ASSERT_EQ(graph_lite_set_double(g.get(), GRAPH_LITE_EDGE, e, "weight", 2.5), GRAPH_LITE_OK);

    graph_lite_value_kind kind = GRAPH_LITE_VALUE_INT;
    ASSERT_EQ(graph_lite_property_kind(g.get(), GRAPH_LITE_EDGE, e, "weight", &kind),
              GRAPH_LITE_OK);
    EXPECT_EQ(kind, GRAPH_LITE_VALUE_DOUBLE);

    double got = 0;
    ASSERT_EQ(graph_lite_get_double(g.get(), GRAPH_LITE_EDGE, e, "weight", &got), GRAPH_LITE_OK);
    EXPECT_EQ(got, 2.5);

    ASSERT_EQ(graph_lite_remove_property(g.get(), GRAPH_LITE_EDGE, e, "weight"), GRAPH_LITE_OK);
    EXPECT_EQ(graph_lite_get_double(g.get(), GRAPH_LITE_EDGE, e, "weight", &got),
              GRAPH_LITE_ERR_NOT_FOUND);
}

TEST(Model, BoolPropertyIsStoredNormalized) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    int got = -1;

    ASSERT_EQ(graph_lite_set_bool(g.get(), GRAPH_LITE_NODE, n, "flag", 42), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_get_bool(g.get(), GRAPH_LITE_NODE, n, "flag", &got), GRAPH_LITE_OK);
    EXPECT_EQ(got, 1);

    ASSERT_EQ(graph_lite_set_bool(g.get(), GRAPH_LITE_NODE, n, "flag", 0), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_get_bool(g.get(), GRAPH_LITE_NODE, n, "flag", &got), GRAPH_LITE_OK);
    EXPECT_EQ(got, 0);

    ASSERT_EQ(graph_lite_set_bool(g.get(), GRAPH_LITE_NODE, n, "flag", -7), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_get_bool(g.get(), GRAPH_LITE_NODE, n, "flag", &got), GRAPH_LITE_OK);
    EXPECT_EQ(got, 1);
}

TEST(Model, StringPropertyIsCopiedAndSizedByLength) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    char source[] = "hello";
    ASSERT_EQ(graph_lite_set_string(g.get(), GRAPH_LITE_NODE, n, "name", source), GRAPH_LITE_OK);
    source[0] = 'J'; // the store holds its own copy

    size_t len = 0;
    EXPECT_EQ(graph_lite_get_string(g.get(), GRAPH_LITE_NODE, n, "name", nullptr, 0, &len),
              GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(len, 5u);

    char tight[5]; // room for the text but not the terminator
    len = 0;
    EXPECT_EQ(graph_lite_get_string(g.get(), GRAPH_LITE_NODE, n, "name", tight, sizeof tight, &len),
              GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(len, 5u);

    char buf[6];
    len = 0;
    ASSERT_EQ(graph_lite_get_string(g.get(), GRAPH_LITE_NODE, n, "name", buf, sizeof buf, &len),
              GRAPH_LITE_OK);
    EXPECT_EQ(len, 5u);
    EXPECT_STREQ(buf, "hello");
}

TEST(Model, EmptyStringValueAndEmptyKeyAreOrdinary) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    ASSERT_EQ(graph_lite_set_string(g.get(), GRAPH_LITE_NODE, n, "", ""), GRAPH_LITE_OK);

    char buf[1] = {'x'};
    size_t len = 99;
    ASSERT_EQ(graph_lite_get_string(g.get(), GRAPH_LITE_NODE, n, "", buf, sizeof buf, &len),
              GRAPH_LITE_OK);
    EXPECT_EQ(len, 0u);
    EXPECT_EQ(buf[0], '\0');
}

TEST(Model, SettingAnExistingKeyOverwritesAndMayChangeKind) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, n, "k", 1), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, n, "k", 2), GRAPH_LITE_OK);
    int64_t i = 0;
    ASSERT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, "k", &i), GRAPH_LITE_OK);
    EXPECT_EQ(i, 2);

    ASSERT_EQ(graph_lite_set_string(g.get(), GRAPH_LITE_NODE, n, "k", "text"), GRAPH_LITE_OK);
    graph_lite_value_kind kind = GRAPH_LITE_VALUE_INT;
    ASSERT_EQ(graph_lite_property_kind(g.get(), GRAPH_LITE_NODE, n, "k", &kind), GRAPH_LITE_OK);
    EXPECT_EQ(kind, GRAPH_LITE_VALUE_STRING);
    EXPECT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, "k", &i),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
}

TEST(Model, GetWithWrongValueKindIsInvalidArgument) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, n, "k", 1), GRAPH_LITE_OK);

    double d = 0;
    int b = 0;
    char buf[8];
    size_t len = 0;
    EXPECT_EQ(graph_lite_get_double(g.get(), GRAPH_LITE_NODE, n, "k", &d),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_get_bool(g.get(), GRAPH_LITE_NODE, n, "k", &b),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_get_string(g.get(), GRAPH_LITE_NODE, n, "k", buf, sizeof buf, &len),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
}

TEST(Model, PropertyOnMissingOwnerOrKeyIsNotFound) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    int64_t got = 0;
    EXPECT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, "absent", &got),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, 9999, "k", 1), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_EDGE, 1, "k", 1), GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, kInvalidId, "k", 1),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, 9999, "k", &got),
              GRAPH_LITE_ERR_NOT_FOUND);
    EXPECT_EQ(graph_lite_remove_property(g.get(), GRAPH_LITE_NODE, 9999, "k"),
              GRAPH_LITE_ERR_NOT_FOUND);
}

TEST(Model, LongPropertyKeysDoNotAllocateDuringReadOrRemoval) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    const std::string key(128, 'k');
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, n, key.c_str(), 42), GRAPH_LITE_OK);

    int64_t value = 0;
    bool threw = false;
    EXPECT_EQ(
        call_with_allocations_disabled(
            [&] { return graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, key.c_str(), &value); },
            &threw),
        GRAPH_LITE_OK);
    EXPECT_FALSE(threw);
    EXPECT_EQ(value, 42);

    graph_lite_value_kind kind = GRAPH_LITE_VALUE_STRING;
    EXPECT_EQ(call_with_allocations_disabled(
                  [&] {
                      return graph_lite_property_kind(g.get(), GRAPH_LITE_NODE, n, key.c_str(),
                                                      &kind);
                  },
                  &threw),
              GRAPH_LITE_OK);
    EXPECT_FALSE(threw);
    EXPECT_EQ(kind, GRAPH_LITE_VALUE_INT);

    EXPECT_EQ(
        call_with_allocations_disabled(
            [&] { return graph_lite_remove_property(g.get(), GRAPH_LITE_NODE, n, key.c_str()); },
            &threw),
        GRAPH_LITE_OK);
    EXPECT_FALSE(threw);
}

TEST(Model, NullArgumentsAndUnknownKindAreInvalidArgument) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    const auto bad_kind = static_cast<graph_lite_kind>(42);
    int64_t got = 0;
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, n, "k", 1), GRAPH_LITE_OK);

    EXPECT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, n, nullptr, 1),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_set_string(g.get(), GRAPH_LITE_NODE, n, "s", nullptr),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, "k", nullptr),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, nullptr, &got),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_property_kind(g.get(), GRAPH_LITE_NODE, n, "k", nullptr),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_remove_property(g.get(), GRAPH_LITE_NODE, n, nullptr),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_get_string(g.get(), GRAPH_LITE_NODE, n, "k", nullptr, 0, nullptr),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);

    EXPECT_EQ(graph_lite_set_int(g.get(), bad_kind, n, "k", 1), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_get_int(g.get(), bad_kind, n, "k", &got), GRAPH_LITE_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(graph_lite_remove_property(g.get(), bad_kind, n, "k"),
              GRAPH_LITE_ERR_INVALID_ARGUMENT);
}

TEST(Model, NodeAndEdgePropertiesLiveInSeparateOwnerSpaces) {
    auto g = make_graph();
    const auto n = add_node(g.get());       // node id 1
    const auto e = add_edge(g.get(), n, n); // edge id 1 — same number, different owner
    ASSERT_EQ(n, e);

    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, n, "k", 10), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_EDGE, e, "k", 20), GRAPH_LITE_OK);

    int64_t got = 0;
    ASSERT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, "k", &got), GRAPH_LITE_OK);
    EXPECT_EQ(got, 10);
    ASSERT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_EDGE, e, "k", &got), GRAPH_LITE_OK);
    EXPECT_EQ(got, 20);

    ASSERT_EQ(graph_lite_remove_property(g.get(), GRAPH_LITE_NODE, n, "k"), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_EDGE, e, "k", &got), GRAPH_LITE_OK);
    EXPECT_EQ(got, 20);
}

TEST(Model, PropertiesDoNotDisturbAdjacencyAndViceVersa) {
    auto g = make_graph();
    const auto a = add_node(g.get());
    const auto b = add_node(g.get());
    const auto e = add_edge(g.get(), a, b);
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, a, "k", 1), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_set_string(g.get(), GRAPH_LITE_EDGE, e, "k", "v"), GRAPH_LITE_OK);
    EXPECT_EQ(out_of(g.get(), a), Ids{e});

    ASSERT_EQ(graph_lite_remove_property(g.get(), GRAPH_LITE_NODE, a, "k"), GRAPH_LITE_OK);
    EXPECT_EQ(out_of(g.get(), a), Ids{e});
    EXPECT_EQ(graph_lite_edge_count(g.get()), 1u);

    // Removing the edge erases its properties but not the endpoint's.
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, a, "k", 5), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_remove_edge(g.get(), e), GRAPH_LITE_OK);
    char buf[4];
    size_t len = 0;
    EXPECT_EQ(graph_lite_get_string(g.get(), GRAPH_LITE_EDGE, e, "k", buf, sizeof buf, &len),
              GRAPH_LITE_ERR_NOT_FOUND);
    int64_t got = 0;
    ASSERT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, a, "k", &got), GRAPH_LITE_OK);
    EXPECT_EQ(got, 5);
}

TEST(Model, RemovedNodePropertiesAreErased) {
    auto g = make_graph();
    const auto n = add_node(g.get());
    ASSERT_EQ(graph_lite_set_int(g.get(), GRAPH_LITE_NODE, n, "k", 1), GRAPH_LITE_OK);
    ASSERT_EQ(graph_lite_remove_node(g.get(), n), GRAPH_LITE_OK);

    int64_t got = 0;
    EXPECT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, n, "k", &got), GRAPH_LITE_ERR_NOT_FOUND);
    // A fresh node never inherits what its predecessor carried.
    const auto fresh = add_node(g.get());
    EXPECT_EQ(graph_lite_get_int(g.get(), GRAPH_LITE_NODE, fresh, "k", &got),
              GRAPH_LITE_ERR_NOT_FOUND);
}
