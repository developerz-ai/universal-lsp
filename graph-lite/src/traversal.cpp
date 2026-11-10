// Native graph traversal at the C ABI seam. The result snapshot owns only ids,
// so it is readable after its source graph changes or is destroyed.
#include "graph-lite.h"

#include "store.hpp"

#include <memory>
#include <unordered_set>
#include <vector>

struct graph_lite_traversal {
    std::vector<graph_lite_id> ids;
    size_t position = 0;
    bool partial = false;
};

namespace {

struct Work {
    graph_lite_id id;
    uint64_t depth;
};

bool valid_order(graph_lite_walk_order order) {
    return order == GRAPH_LITE_WALK_BREADTH_FIRST || order == GRAPH_LITE_WALK_DEPTH_FIRST;
}

bool valid_direction(graph_lite_walk_direction direction) {
    return direction == GRAPH_LITE_WALK_FORWARD || direction == GRAPH_LITE_WALK_REVERSE;
}

template <typename Fn>
graph_lite_status guarded(Fn&& fn) {
    try {
        return fn();
    } catch (...) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    }
}

graph_lite_status build_walk(const graph_lite_graph& graph, graph_lite_id start,
                             graph_lite_walk_order order, graph_lite_walk_direction direction,
                             int filter_node_tag, graph_lite_tag node_tag, int filter_edge_tag,
                             graph_lite_tag edge_tag, uint64_t max_depth, uint64_t max_results,
                             graph_lite_traversal& traversal) {
    if (!graph.store.has_node(start))
        return GRAPH_LITE_ERR_NOT_FOUND;

    std::unordered_set<graph_lite_id> discovered;
    std::unordered_set<graph_lite_id> depth_blocked;
    std::vector<Work> work;
    discovered.insert(start);
    if (max_results == 0) {
        traversal.partial = true;
        return GRAPH_LITE_OK;
    }
    work.push_back({start, 0});
    uint64_t admitted = 1;

    size_t next = 0;
    while (order == GRAPH_LITE_WALK_BREADTH_FIRST ? next < work.size() : !work.empty()) {
        Work current{};
        if (order == GRAPH_LITE_WALK_BREADTH_FIRST) {
            current = work[next++];
        } else {
            current = work.back();
            work.pop_back();
        }
        traversal.ids.push_back(current.id);

        const bool reverse = direction == GRAPH_LITE_WALK_REVERSE;
        uint64_t edge_count = 0;
        graph_lite_status status =
            reverse ? graph.store.in_edge_ids(current.id, nullptr, 0, &edge_count)
                    : graph.store.out_edge_ids(current.id, nullptr, 0, &edge_count);
        if (status != GRAPH_LITE_OK && status != GRAPH_LITE_ERR_BUFFER_TOO_SMALL)
            return status;
        std::vector<graph_lite_id> edges(edge_count);
        status = reverse
                     ? graph.store.in_edge_ids(current.id, edges.data(), edge_count, &edge_count)
                     : graph.store.out_edge_ids(current.id, edges.data(), edge_count, &edge_count);
        if (status != GRAPH_LITE_OK)
            return status;
        for (const graph_lite_id edge : edges) {
            graph_lite_id candidate = 0;
            graph_lite_tag candidate_edge_tag = 0;
            graph_lite_id from = 0;
            graph_lite_id to = 0;
            status = graph.store.edge_info(edge, &from, &to, &candidate_edge_tag);
            if (status != GRAPH_LITE_OK)
                return status;
            candidate = reverse ? from : to;
            if ((filter_edge_tag != 0 && candidate_edge_tag != edge_tag) ||
                discovered.contains(candidate))
                continue;
            graph_lite_tag candidate_node_tag = 0;
            status = graph.store.node_type(candidate, &candidate_node_tag);
            if (status != GRAPH_LITE_OK)
                return status;
            if (filter_node_tag != 0 && candidate_node_tag != node_tag)
                continue;
            if (current.depth >= max_depth) {
                depth_blocked.insert(candidate);
                continue;
            }
            if (admitted >= max_results) {
                traversal.partial = true;
                continue;
            }
            discovered.insert(candidate);
            ++admitted;
            work.push_back({candidate, current.depth + 1});
        }
    }
    for (const graph_lite_id candidate : depth_blocked)
        if (!discovered.contains(candidate))
            traversal.partial = true;
    return GRAPH_LITE_OK;
}

} // namespace

extern "C" {

graph_lite_status graph_lite_traversal_open(const graph_lite_graph* graph, graph_lite_id start,
                                            graph_lite_walk_order order,
                                            graph_lite_walk_direction direction,
                                            int filter_node_tag, graph_lite_tag node_tag,
                                            int filter_edge_tag, graph_lite_tag edge_tag,
                                            uint64_t max_depth, uint64_t max_results,
                                            graph_lite_traversal** out) {
    if (!graph || !out || !valid_order(order) || !valid_direction(direction))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] {
        auto traversal = std::make_unique<graph_lite_traversal>();
        const graph_lite_status status =
            build_walk(*graph, start, order, direction, filter_node_tag, node_tag, filter_edge_tag,
                       edge_tag, max_depth, max_results, *traversal);
        if (status != GRAPH_LITE_OK)
            return status;
        *out = traversal.release();
        return GRAPH_LITE_OK;
    });
}

graph_lite_status graph_lite_traversal_next(graph_lite_traversal* traversal,
                                            graph_lite_id* out_id) {
    if (!traversal || !out_id)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    if (traversal->position >= traversal->ids.size())
        return GRAPH_LITE_ERR_END;
    *out_id = traversal->ids[traversal->position++];
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_traversal_partial(const graph_lite_traversal* traversal,
                                               int* out_partial) {
    if (!traversal || !out_partial)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_partial = traversal->partial ? 1 : 0;
    return GRAPH_LITE_OK;
}

void graph_lite_traversal_destroy(graph_lite_traversal* traversal) {
    delete traversal;
}

} // extern "C"
