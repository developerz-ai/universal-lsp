// graph-lite — cursors and shortest path.
//
// cursor_open performs the whole walk eagerly and the cursor keeps only the
// resulting steps: a cursor is a snapshot, valid no matter what later happens
// to the store. The walk marks a node visited the moment it is discovered,
// follows each node's adjacency in insertion order, and reports truncated=1
// exactly when a depth or result bound refused a node the unbounded walk
// would have reached. Filters shape the walk (edge types) and its returned
// nodes (node types); they are never cuts.

#include "store_internal.hpp"

#include <new>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using graph_lite::internal::Cursor;
using graph_lite::internal::EdgeRecord;
using graph_lite::internal::NodeRecord;
using graph_lite::internal::Step;
using graph_lite::internal::Store;

/// Is `type` admitted by a filter list? NULL+0 means "any" — the entry
/// points validate the pair, so a NULL list here is the unfiltered case.
template <typename T>
bool type_admitted(const T* types, uint64_t count, T type) {
    if (types == nullptr)
        return true;
    for (uint64_t i = 0; i < count; ++i)
        if (types[i] == type)
            return true;
    return false;
}

/// Visits every edge of `node` the edge-type filter admits, in insertion
/// order, as (other endpoint, edge id). Direction picks which end is "the
/// other one".
template <typename Visit>
void for_each_neighbour(const Store& store, const NodeRecord& node, graph_lite_direction direction,
                        const graph_lite_edge_type* edge_types, uint64_t edge_type_count,
                        Visit&& visit) {
    const auto& edge_ids = direction == GRAPH_LITE_OUTGOING ? node.out_edges : node.in_edges;
    for (const graph_lite_edge_id id : edge_ids) {
        const EdgeRecord& edge = *store.edge(id);
        if (!type_admitted(edge_types, edge_type_count, edge.type))
            continue;
        visit(direction == GRAPH_LITE_OUTGOING ? edge.to : edge.from, id);
    }
}

/// One pending node: where it lives, how deep in the walk tree it sits, and
/// the edge (and node) the walk arrives through. A node is marked visited
/// when its frame is DISCOVERED, but only RETURNED when the frame is popped
/// — that is what makes the depth-first walk come out in preorder.
struct Frame {
    size_t node_slot;
    uint64_t depth;
    graph_lite_edge_id edge; // graph_lite::internal::kNoEdge on the start frame
    graph_lite_node_id from; // == node on the start frame
};

/// The whole walk, eagerly. Appends one Step per returned node — the start
/// node first, exempt from the node filter — and sets `truncated` exactly
/// when a bound refused a node the unbounded walk would have returned.
void run_walk(const Store& store, const NodeRecord& start, graph_lite_order order,
              graph_lite_direction direction, uint64_t max_depth, uint64_t max_results,
              const graph_lite_edge_type* edge_types, uint64_t edge_type_count,
              const graph_lite_node_type* node_types, uint64_t node_type_count,
              std::vector<Step>& steps, bool& truncated) {
    const size_t start_slot = store.node_slot.at(start.id);
    std::unordered_set<graph_lite_node_id> visited;
    visited.insert(start.id);
    std::vector<Frame> frontier{Frame{start_slot, 0, graph_lite::internal::kNoEdge, start.id}};

    // Discovers the unvisited neighbours of `frame`, in adjacency order.
    const auto expand = [&](const Frame& frame, std::vector<Frame>& discovered) {
        const NodeRecord& node = store.nodes[frame.node_slot];
        for_each_neighbour(store, node, direction, edge_types, edge_type_count,
                           [&](graph_lite_node_id target, graph_lite_edge_id edge) {
                               if (visited.contains(target))
                                   return;
                               const size_t target_slot = store.node_slot.at(target);
                               visited.insert(target);
                               discovered.push_back(
                                   Frame{target_slot, frame.depth + 1, edge, node.id});
                           });
    };

    // Returns one pending node to the walk: appends its Step when it passes
    // the node filter, and expands it unless a depth bound stops the descent.
    // Returns false once a result bound has provably cut the walk short.
    const auto visit = [&](const Frame& frame, std::vector<Frame>& discovered) {
        const NodeRecord& node = store.nodes[frame.node_slot];
        // Only the start frame arrives via no edge, and it alone is exempt
        // from the node filter.
        const bool emits = frame.edge == graph_lite::internal::kNoEdge ||
                           type_admitted(node_types, node_type_count, node.type);
        if (emits && max_results > 0 && steps.size() >= max_results) {
            truncated = true;
            return false;
        }
        if (emits)
            steps.push_back(Step{node.id, frame.depth, frame.edge, frame.from});
        if (max_depth > 0 && frame.depth == max_depth) {
            // Expanding would create steps deeper than the cap. The bound
            // cuts the walk here only if such a step was actually reachable.
            for_each_neighbour(store, node, direction, edge_types, edge_type_count,
                               [&](graph_lite_node_id target, graph_lite_edge_id) {
                                   if (!visited.contains(target))
                                       truncated = true;
                               });
            return true;
        }
        expand(frame, discovered);
        return true;
    };

    if (order == GRAPH_LITE_BREADTH_FIRST) {
        // Index scan over a growing vector: exactly the FIFO by level.
        for (size_t head = 0; head < frontier.size(); ++head) {
            std::vector<Frame> discovered;
            if (!visit(frontier[head], discovered))
                break;
            frontier.insert(frontier.end(), discovered.begin(), discovered.end());
        }
    } else {
        // Preorder: descend into the first-discovered neighbour fully before
        // the next one. The stack runs backwards, so discovery order lands
        // on it reversed.
        while (!frontier.empty()) {
            const Frame frame = frontier.back();
            frontier.pop_back();
            std::vector<Frame> discovered;
            if (!visit(frame, discovered))
                break;
            for (auto it = discovered.rbegin(); it != discovered.rend(); ++it)
                frontier.push_back(*it);
        }
    }
}

} // namespace

extern "C" {

graph_lite_status graph_lite_cursor_open(const graph_lite_store* store, graph_lite_node_id start,
                                         graph_lite_order order, graph_lite_direction direction,
                                         uint64_t max_depth, uint64_t max_results,
                                         const graph_lite_edge_type* edge_types,
                                         uint64_t edge_type_count,
                                         const graph_lite_node_type* node_types,
                                         uint64_t node_type_count, graph_lite_cursor** out_cursor) {
    if (store == nullptr || out_cursor == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_cursor = nullptr;
    if (order != GRAPH_LITE_BREADTH_FIRST && order != GRAPH_LITE_DEPTH_FIRST)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    if (direction != GRAPH_LITE_OUTGOING && direction != GRAPH_LITE_INCOMING)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    // Filters come as pointer+count pairs: both set, or neither.
    if ((edge_types == nullptr) != (edge_type_count == 0))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    if ((node_types == nullptr) != (node_type_count == 0))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const auto* self = reinterpret_cast<const Store*>(store);
    try {
        const NodeRecord* const start_node = self->node(start);
        if (start_node == nullptr)
            return GRAPH_LITE_ERR_NOT_FOUND;
        std::vector<Step> steps;
        bool truncated = false;
        run_walk(*self, *start_node, order, direction, max_depth, max_results, edge_types,
                 edge_type_count, node_types, node_type_count, steps, truncated);
        Cursor* cursor = new (std::nothrow) Cursor{std::move(steps), 0, truncated};
        if (cursor == nullptr)
            return GRAPH_LITE_ERR_OUT_OF_MEMORY;
        *out_cursor = reinterpret_cast<graph_lite_cursor*>(cursor);
        return GRAPH_LITE_OK;
    } catch (const std::bad_alloc&) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        // Unreachable by construction: the only exceptions internal code can
        // raise are invariant breaks, and corrupt is what that is.
        return GRAPH_LITE_ERR_CORRUPT;
    }
}

graph_lite_status graph_lite_cursor_next(graph_lite_cursor* cursor, graph_lite_node_id* out_node) {
    if (cursor == nullptr || out_node == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<Cursor*>(cursor);
    if (self->position >= self->steps.size())
        return GRAPH_LITE_DONE;
    *out_node = self->steps[self->position].node;
    ++self->position;
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_cursor_depth(const graph_lite_cursor* cursor, uint64_t* out_depth) {
    if (cursor == nullptr || out_depth == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const auto* self = reinterpret_cast<const Cursor*>(cursor);
    *out_depth = graph_lite::internal::current(*self).depth;
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_cursor_arrived_via(const graph_lite_cursor* cursor,
                                                graph_lite_edge_id* out_edge,
                                                graph_lite_node_id* out_from) {
    if (cursor == nullptr || out_edge == nullptr || out_from == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const auto* self = reinterpret_cast<const Cursor*>(cursor);
    const Step& step = graph_lite::internal::current(*self);
    *out_edge = step.edge;
    *out_from = step.from;
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_cursor_truncated(const graph_lite_cursor* cursor, int* out_truncated) {
    if (cursor == nullptr || out_truncated == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_truncated = reinterpret_cast<const Cursor*>(cursor)->truncated ? 1 : 0;
    return GRAPH_LITE_OK;
}

void graph_lite_cursor_free(graph_lite_cursor* cursor) {
    delete reinterpret_cast<Cursor*>(cursor);
}

graph_lite_status graph_lite_shortest_path(const graph_lite_store* store, graph_lite_node_id from,
                                           graph_lite_node_id to,
                                           const graph_lite_edge_type* edge_types,
                                           uint64_t edge_type_count, graph_lite_node_id* out_path,
                                           uint64_t capacity, uint64_t* out_length,
                                           uint64_t* out_required) {
    if (store == nullptr || out_length == nullptr || out_required == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    // out_path and capacity are a pair too: a NULL buffer means the size
    // query, and capacity 0 is the only capacity a NULL buffer can have.
    if ((out_path == nullptr) != (capacity == 0))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    // The edge filter is a pointer+count pair as well — both set, or neither
    // — exactly as cursor_open requires it before any walk may start.
    if ((edge_types == nullptr) != (edge_type_count == 0))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const auto* self = reinterpret_cast<const Store*>(store);
    try {
        if (self->node(from) == nullptr || self->node(to) == nullptr)
            return GRAPH_LITE_ERR_NOT_FOUND;
        // BFS over outgoing edges; first discovery fixes the parent, so
        // equal-length ties resolve in insertion order.
        std::unordered_map<graph_lite_node_id, std::pair<graph_lite_node_id, graph_lite_edge_id>>
            parent;
        std::unordered_set<graph_lite_node_id> visited{from};
        std::vector<graph_lite_node_id> queue{from};
        bool found = from == to;
        for (size_t head = 0; head < queue.size() && !found; ++head) {
            const NodeRecord& node = *self->node(queue[head]);
            for (const graph_lite_edge_id id : node.out_edges) {
                const EdgeRecord& edge = *self->edge(id);
                if (!type_admitted(edge_types, edge_type_count, edge.type))
                    continue;
                if (!visited.insert(edge.to).second)
                    continue;
                parent.emplace(edge.to, std::make_pair(queue[head], id));
                if (edge.to == to) {
                    found = true;
                    break;
                }
                queue.push_back(edge.to);
            }
        }
        if (!found)
            return GRAPH_LITE_ERR_UNREACHABLE;
        uint64_t length = 0;
        for (graph_lite_node_id at = to;; at = parent.at(at).first) {
            ++length;
            if (at == from)
                break;
        }
        *out_length = length;
        *out_required = length;
        if (out_path == nullptr)
            return GRAPH_LITE_OK; // the size query: *out_required is the answer
        if (capacity < length)
            return GRAPH_LITE_ERR_BUFFER_TOO_SMALL; // buffer stays untouched
        graph_lite_node_id at = to;
        for (uint64_t i = length; i > 0; --i) {
            out_path[i - 1] = at;
            const auto arrived = parent.find(at);
            at = arrived == parent.end() ? from : arrived->second.first;
        }
        return GRAPH_LITE_OK;
    } catch (const std::bad_alloc&) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        // Unreachable by construction: the only exceptions internal code can
        // raise are invariant breaks, and corrupt is what that is.
        return GRAPH_LITE_ERR_CORRUPT;
    }
}

} // extern "C"
