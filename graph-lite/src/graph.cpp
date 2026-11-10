// The C ABI seam for the store-query surface: validates exactly what the
// internal Store cannot see — null handles and null out-parameters — then
// delegates. Identity (unknown ids, missing endpoints) is decided inside
// Store, next to the indexes it owns; one seam per invariant keeps "where
// would this be checked" answerable, and this file stays thin enough to audit
// by eye.
//
// Every entry point here is read-only over materialized state: none allocates,
// so none can fail with OOM. The mutating model surface lives in model.cpp;
// its guarded() is what turns allocation failure into the ABI's OOM status.
#include "graph-lite.h"

#include "store.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace {

// Footprint estimate for one property table: its hash index at bucket
// granularity plus, per owner, the owner's property map at bucket granularity.
// Allocator overhead and string capacity are ignored — capacity planning, not
// accounting, matching the store side of the estimate.
uint64_t properties_bytes(const graph_lite::internal::PropertyTable& table) {
    uint64_t bytes = table.bucket_count() *
                     sizeof(std::pair<const graph_lite_id, graph_lite::internal::PropertyMap>);
    for (const auto& [owner, properties] : table)
        bytes += properties.bucket_count() *
                 sizeof(std::pair<const std::string, graph_lite::internal::PropertyValue>);
    return bytes;
}

} // namespace

extern "C" {

graph_lite_status graph_lite_graph_memory_usage(const graph_lite_graph* graph,
                                                uint64_t* out_bytes) {
    if (graph == nullptr || out_bytes == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_bytes = graph->store.memory_usage() + properties_bytes(graph->node_properties) +
                 properties_bytes(graph->edge_properties);
    return GRAPH_LITE_OK;
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

} // extern "C"
