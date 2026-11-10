// Shared helpers for the model_*_test.cpp suites: an RAII graph handle and thin
// wrappers over the C header that assert success. Header-only, inline.
#pragma once

#include "graph-lite.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <vector>

struct GraphDeleter {
    void operator()(graph_lite_graph* g) const { graph_lite_graph_destroy(g); }
};

using GraphPtr = std::unique_ptr<graph_lite_graph, GraphDeleter>;
using Ids = std::vector<graph_lite_id>;

// The header's GRAPH_LITE_INVALID_ID is a C-style cast, which -Werror=old-style-cast
// rejects when expanded in C++.
inline constexpr graph_lite_id kInvalidId = 0;

inline GraphPtr make_graph() {
    graph_lite_graph* raw = nullptr;
    EXPECT_EQ(graph_lite_graph_create(&raw), GRAPH_LITE_OK);
    return GraphPtr(raw);
}

inline graph_lite_id add_node(graph_lite_graph* g, graph_lite_tag tag = 1) {
    graph_lite_id id = kInvalidId;
    EXPECT_EQ(graph_lite_add_node(g, tag, &id), GRAPH_LITE_OK);
    return id;
}

inline graph_lite_id add_edge(graph_lite_graph* g, graph_lite_id from, graph_lite_id to,
                              graph_lite_tag tag = 1) {
    graph_lite_id id = kInvalidId;
    EXPECT_EQ(graph_lite_add_edge(g, from, to, tag, &id), GRAPH_LITE_OK);
    return id;
}

using ListFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_id, graph_lite_id*, size_t,
                                     size_t*);

inline Ids list(ListFn fn, const graph_lite_graph* g, graph_lite_id node) {
    size_t need = 0;
    const graph_lite_status probe = fn(g, node, nullptr, 0, &need);
    if (need == 0) {
        EXPECT_EQ(probe, GRAPH_LITE_OK);
        return {};
    }
    EXPECT_EQ(probe, GRAPH_LITE_ERR_BUFFER_TOO_SMALL);
    Ids ids(need);
    size_t count = 0;
    EXPECT_EQ(fn(g, node, ids.data(), ids.size(), &count), GRAPH_LITE_OK);
    EXPECT_EQ(count, need);
    return ids;
}

inline Ids out_of(const graph_lite_graph* g, graph_lite_id n) {
    return list(graph_lite_out_edges, g, n);
}

inline Ids in_of(const graph_lite_graph* g, graph_lite_id n) {
    return list(graph_lite_in_edges, g, n);
}
