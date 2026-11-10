// Cursor entry points of graph-lite.h. A cursor is a snapshot of ids taken at
// open: it owns a copy, so it is independent of later mutation and of the
// graph's lifetime. Layout is private — the header only forward-declares it.
#include "graph-lite.h"

#include "store.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

struct graph_lite_cursor {
    std::vector<graph_lite_id> ids;
    size_t pos = 0;
};

namespace {

// Nothing may unwind across extern "C". Snapshotting allocates, and the
// standard library can raise (a size limit surfaces as length_error) — anything
// escaping maps to OOM, as in model.cpp.
template <typename Fn>
graph_lite_status guarded(Fn&& fn) {
    try {
        return fn();
    } catch (...) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    }
}

using graph_lite::internal::Store;

using ListFn = graph_lite_status (Store::*)(graph_lite_id, graph_lite_id*, uint64_t,
                                            uint64_t*) const;

// Snapshot one node's adjacency: size probe (nullptr, 0), then a sized copy.
// A probe answering OK means an empty list; BUFFER_TOO_SMALL carries the size.
graph_lite_status snapshot_adjacency(const Store& store, ListFn list, graph_lite_id node,
                                     std::vector<graph_lite_id>& ids) {
    uint64_t required = 0;
    graph_lite_status status = (store.*list)(node, nullptr, 0, &required);
    if (status == GRAPH_LITE_OK)
        return GRAPH_LITE_OK;
    if (status != GRAPH_LITE_ERR_BUFFER_TOO_SMALL)
        return status;
    ids.resize(static_cast<size_t>(required));
    uint64_t written = 0;
    return (store.*list)(node, ids.data(), required, &written);
}

graph_lite_status snapshot(const Store& store, graph_lite_cursor_kind kind, graph_lite_id node,
                           std::vector<graph_lite_id>& ids) {
    switch (kind) {
    case GRAPH_LITE_CURSOR_NODES:
        store.collect_node_ids(ids);
        std::sort(ids.begin(), ids.end());
        return GRAPH_LITE_OK;
    case GRAPH_LITE_CURSOR_EDGES:
        store.collect_edge_ids(ids);
        std::sort(ids.begin(), ids.end());
        return GRAPH_LITE_OK;
    case GRAPH_LITE_CURSOR_OUT_EDGES:
        return snapshot_adjacency(store, &Store::out_edge_ids, node, ids);
    case GRAPH_LITE_CURSOR_IN_EDGES:
        return snapshot_adjacency(store, &Store::in_edge_ids, node, ids);
    case GRAPH_LITE_CURSOR_OUT_NEIGHBORS:
        return snapshot_adjacency(store, &Store::out_neighbors, node, ids);
    case GRAPH_LITE_CURSOR_IN_NEIGHBORS:
        return snapshot_adjacency(store, &Store::in_neighbors, node, ids);
    case GRAPH_LITE_CURSOR_KIND_FORCE_INT:
        break;
    }
    return GRAPH_LITE_ERR_INVALID_ARGUMENT;
}

bool known_kind(graph_lite_cursor_kind kind) {
    const int k = static_cast<int>(kind);
    return k >= GRAPH_LITE_CURSOR_NODES && k <= GRAPH_LITE_CURSOR_IN_NEIGHBORS;
}

} // namespace

extern "C" {

graph_lite_status graph_lite_cursor_open(const graph_lite_graph* g, graph_lite_cursor_kind kind,
                                         graph_lite_id node, graph_lite_cursor** out) {
    if (!g || !out || !known_kind(kind))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] {
        auto cursor = std::make_unique<graph_lite_cursor>();
        const graph_lite_status status = snapshot(g->store, kind, node, cursor->ids);
        if (status != GRAPH_LITE_OK)
            return status;
        *out = cursor.release();
        return GRAPH_LITE_OK;
    });
}

graph_lite_status graph_lite_cursor_next(graph_lite_cursor* c, graph_lite_id* out_id) {
    if (!c || !out_id)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    if (c->pos >= c->ids.size())
        return GRAPH_LITE_ERR_END;
    *out_id = c->ids[c->pos++];
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_cursor_drain(graph_lite_cursor* c, graph_lite_id* buf, uint64_t cap,
                                          uint64_t* count) {
    if (!c || !count || (!buf && cap > 0))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const uint64_t remaining = static_cast<uint64_t>(c->ids.size() - c->pos);
    *count = remaining;
    if (cap < remaining)
        return GRAPH_LITE_ERR_BUFFER_TOO_SMALL;
    std::copy(c->ids.begin() + static_cast<std::ptrdiff_t>(c->pos), c->ids.end(), buf);
    c->pos = c->ids.size();
    return GRAPH_LITE_OK;
}

void graph_lite_cursor_destroy(graph_lite_cursor* c) {
    delete c;
}

} // extern "C"
