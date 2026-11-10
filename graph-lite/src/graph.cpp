// The C ABI seam: validates exactly what the internal Store cannot see — null
// handles and null out-parameters — then delegates. Identity (unknown ids,
// missing endpoints) is decided inside Store, next to the indexes it owns; one
// seam per invariant keeps "where would this be checked" answerable, and this
// file stays thin enough to audit by eye.
// The seam is also the only place C++ meets the one thing a C caller cannot do:
// catch. The store allocates, and its allocations throw; every entry point that
// can allocate funnels through guarded() so the failure surfaces as the ABI's
// OOM status instead of unwinding into a caller that has no way to handle it.
#include "graph-lite.h"

#include "store.hpp"

#include <new>

namespace {

// One handler covers the class: everything that can throw under a store call is
// a container allocation (bad_alloc, or length_error at a capacity limit). The
// catch-all still beats an unwind across a boundary no C caller can catch —
// mislabeling an impossible non-allocation throw as OOM is the honest failure
// mode, because the caller has no third option.
template <typename Fn>
graph_lite_status guarded(Fn&& body) noexcept {
    try {
        return body();
    } catch (...) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    }
}

} // namespace

extern "C" {

graph_lite_graph* graph_lite_graph_create(void) {
    // nothrow new covers only the raw allocation; a construction that itself
    // allocates must still produce NULL — the header promises NULL only on
    // allocation failure, never an escape.
    try {
        return new (std::nothrow) graph_lite_graph();
    } catch (...) {
        return nullptr;
    }
}

void graph_lite_graph_destroy(graph_lite_graph* graph) {
    delete graph; // deleting null is a no-op
}

graph_lite_status graph_lite_graph_node_count(const graph_lite_graph* graph, uint64_t* out_count) {
    if (graph == nullptr || out_count == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_count = graph->store.node_count();
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_graph_edge_count(const graph_lite_graph* graph, uint64_t* out_count) {
    if (graph == nullptr || out_count == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_count = graph->store.edge_count();
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_graph_memory_usage(const graph_lite_graph* graph,
                                                uint64_t* out_bytes) {
    if (graph == nullptr || out_bytes == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_bytes = graph->store.memory_usage();
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_node_add(graph_lite_graph* graph, uint64_t type,
                                      graph_lite_id* out_id) {
    if (graph == nullptr || out_id == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] { return graph->store.node_add(type, out_id); });
}

graph_lite_status graph_lite_node_remove(graph_lite_graph* graph, graph_lite_id id) {
    if (graph == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] { return graph->store.node_remove(id); });
}

graph_lite_status graph_lite_node_type(const graph_lite_graph* graph, graph_lite_id id,
                                       uint64_t* out_type) {
    if (graph == nullptr || out_type == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return graph->store.node_type(id, out_type);
}

graph_lite_status graph_lite_node_out_degree(const graph_lite_graph* graph, graph_lite_id id,
                                             uint64_t* out_degree) {
    if (graph == nullptr || out_degree == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return graph->store.out_degree(id, out_degree);
}

graph_lite_status graph_lite_node_in_degree(const graph_lite_graph* graph, graph_lite_id id,
                                            uint64_t* out_degree) {
    if (graph == nullptr || out_degree == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return graph->store.in_degree(id, out_degree);
}

graph_lite_status graph_lite_node_out_neighbors(const graph_lite_graph* graph, graph_lite_id id,
                                                graph_lite_id* buffer, uint64_t capacity,
                                                uint64_t* out_count) {
    if (graph == nullptr || out_count == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return graph->store.out_neighbors(id, buffer, capacity, out_count);
}

graph_lite_status graph_lite_node_in_neighbors(const graph_lite_graph* graph, graph_lite_id id,
                                               graph_lite_id* buffer, uint64_t capacity,
                                               uint64_t* out_count) {
    if (graph == nullptr || out_count == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return graph->store.in_neighbors(id, buffer, capacity, out_count);
}

graph_lite_status graph_lite_edge_add(graph_lite_graph* graph, graph_lite_id from, graph_lite_id to,
                                      uint64_t type, graph_lite_id* out_id) {
    if (graph == nullptr || out_id == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] { return graph->store.edge_add(from, to, type, out_id); });
}

graph_lite_status graph_lite_edge_remove(graph_lite_graph* graph, graph_lite_id id) {
    if (graph == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return graph->store.edge_remove(id);
}

graph_lite_status graph_lite_edge_type(const graph_lite_graph* graph, graph_lite_id id,
                                       uint64_t* out_type) {
    if (graph == nullptr || out_type == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return graph->store.edge_type(id, out_type);
}

} // extern "C"
