// graph-lite — the store entry points, exactly as graph-lite.h declares them.
//
// Two seams do all the guarding. Every entry point validates its handles and
// out-parameters before touching anything, and every allocating entry point
// converts bad_alloc to ERR_OUT_OF_MEMORY so no exception crosses the C
// boundary. Ids come from monotonic counters: the slot vectors recycle
// slots, never numbers.

#include "store_internal.hpp"

#include <new>
#include <utility>
#include <vector>

namespace graph_lite::internal {

NodeRecord* Store::node(graph_lite_node_id id) {
    const auto slot = node_slot.find(id);
    return slot == node_slot.end() ? nullptr : &nodes[slot->second];
}

EdgeRecord* Store::edge(graph_lite_edge_id id) {
    const auto slot = edge_slot.find(id);
    return slot == edge_slot.end() ? nullptr : &edges[slot->second];
}

const NodeRecord* Store::node(graph_lite_node_id id) const {
    const auto slot = node_slot.find(id);
    return slot == node_slot.end() ? nullptr : &nodes[slot->second];
}

const EdgeRecord* Store::edge(graph_lite_edge_id id) const {
    const auto slot = edge_slot.find(id);
    return slot == edge_slot.end() ? nullptr : &edges[slot->second];
}

} // namespace graph_lite::internal

namespace {

using graph_lite::internal::EdgeRecord;
using graph_lite::internal::NodeRecord;
using graph_lite::internal::Store;

/// Reserves a slot for `record`, recycling a freed one when one exists.
template <typename Record>
size_t take_slot(std::vector<Record>& records, std::vector<size_t>& free_slots, Record&& record) {
    if (!free_slots.empty()) {
        const size_t slot = free_slots.back();
        free_slots.pop_back();
        records[slot] = std::move(record);
        return slot;
    }
    records.push_back(std::move(record));
    return records.size() - 1;
}

} // namespace

extern "C" {

graph_lite_status graph_lite_store_create(graph_lite_store** out_store) {
    if (out_store == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_store = nullptr;
    Store* store = new (std::nothrow) Store();
    if (store == nullptr)
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    *out_store = reinterpret_cast<graph_lite_store*>(store);
    return GRAPH_LITE_OK;
}

void graph_lite_store_destroy(graph_lite_store* store) {
    delete reinterpret_cast<Store*>(store);
}

graph_lite_status graph_lite_store_add_node(graph_lite_store* store, graph_lite_node_type type,
                                            graph_lite_node_id* out_id) {
    if (store == nullptr || out_id == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<Store*>(store);
    try {
        const graph_lite_node_id id = self->next_node_id++;
        const size_t slot =
            take_slot(self->nodes, self->free_node_slots, NodeRecord{id, type, {}, {}});
        self->node_slot.emplace(id, slot);
        ++self->node_count;
        *out_id = id;
        return GRAPH_LITE_OK;
    } catch (const std::bad_alloc&) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        // Unreachable by construction: the only exceptions internal code can
        // raise are invariant breaks, and corrupt is what that is.
        return GRAPH_LITE_ERR_CORRUPT;
    }
}

graph_lite_status graph_lite_store_add_edge(graph_lite_store* store, graph_lite_node_id from,
                                            graph_lite_node_id to, graph_lite_edge_type type,
                                            graph_lite_edge_id* out_id) {
    if (store == nullptr || out_id == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<Store*>(store);
    const auto from_slot = self->node_slot.find(from);
    const auto to_slot = self->node_slot.find(to);
    if (from_slot == self->node_slot.end() || to_slot == self->node_slot.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    try {
        const graph_lite_edge_id id = self->next_edge_id++;
        const size_t slot =
            take_slot(self->edges, self->free_edge_slots, EdgeRecord{id, from, to, type});
        self->edge_slot.emplace(id, slot);
        self->nodes[from_slot->second].out_edges.push_back(id);
        self->nodes[to_slot->second].in_edges.push_back(id);
        ++self->edge_count;
        *out_id = id;
        return GRAPH_LITE_OK;
    } catch (const std::bad_alloc&) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        // Unreachable by construction: the only exceptions internal code can
        // raise are invariant breaks, and corrupt is what that is.
        return GRAPH_LITE_ERR_CORRUPT;
    }
}

graph_lite_status graph_lite_store_remove_node(graph_lite_store* store, graph_lite_node_id id) {
    if (store == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<Store*>(store);
    const auto slot = self->node_slot.find(id);
    if (slot == self->node_slot.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    try {
        // Copy first: dropping each incident edge mutates these very lists.
        // A self-loop lands in the copy twice; the second drop finds nothing
        // and is harmless.
        const NodeRecord& node = self->nodes[slot->second];
        std::vector<graph_lite_edge_id> incident = node.out_edges;
        incident.insert(incident.end(), node.in_edges.begin(), node.in_edges.end());
        for (const graph_lite_edge_id edge : incident) {
            const graph_lite_status dropped = graph_lite_store_remove_edge(store, edge);
            // A self-loop sits in the copy twice; the second drop answers
            // NOT_FOUND and is the expected no-op. Any other failure — the
            // slot reserve inside remove_edge — stops the cascade before the
            // node leaves with edges still attached to it.
            if (dropped != GRAPH_LITE_OK && dropped != GRAPH_LITE_ERR_NOT_FOUND)
                return dropped;
        }
        const size_t freed = slot->second;
        self->node_slot.erase(slot);
        self->free_node_slots.push_back(freed);
        --self->node_count;
        return GRAPH_LITE_OK;
    } catch (const std::bad_alloc&) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        // Unreachable by construction: the only exceptions internal code can
        // raise are invariant breaks, and corrupt is what that is.
        return GRAPH_LITE_ERR_CORRUPT;
    }
}

graph_lite_status graph_lite_store_remove_edge(graph_lite_store* store, graph_lite_edge_id id) {
    if (store == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<Store*>(store);
    const auto slot = self->edge_slot.find(id);
    if (slot == self->edge_slot.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    const EdgeRecord& record = self->edges[slot->second];
    NodeRecord* const from = self->node(record.from);
    NodeRecord* const to = self->node(record.to);
    try {
        // The recycled slot is pushed before anything mutates: this is the
        // only step here that can throw, and taking it first keeps the map
        // erase and the endpoint-list updates one all-or-nothing step — a
        // throw can never leave a live id in an adjacency list whose edge is
        // already gone from the map.
        self->free_edge_slots.push_back(slot->second);
        // A live edge id appears exactly once in each endpoint's list — in
        // both lists of the same node when it is a self-loop. Invariant: an
        // edge only ever attaches live nodes, because remove_node drops its
        // incident edges before it drops itself, and add_edge requires both
        // endpoints to exist.
        self->edge_slot.erase(slot);
        std::erase(from->out_edges, id);
        std::erase(to->in_edges, id);
        --self->edge_count;
        return GRAPH_LITE_OK;
    } catch (const std::bad_alloc&) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        // Unreachable by construction: the only exceptions internal code can
        // raise are invariant breaks, and corrupt is what that is.
        return GRAPH_LITE_ERR_CORRUPT;
    }
}

graph_lite_status graph_lite_store_node_count(const graph_lite_store* store, uint64_t* out_count) {
    if (store == nullptr || out_count == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_count = reinterpret_cast<const Store*>(store)->node_count;
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_store_edge_count(const graph_lite_store* store, uint64_t* out_count) {
    if (store == nullptr || out_count == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out_count = reinterpret_cast<const Store*>(store)->edge_count;
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_store_node_type(const graph_lite_store* store, graph_lite_node_id id,
                                             graph_lite_node_type* out_type) {
    if (store == nullptr || out_type == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const NodeRecord* const node = reinterpret_cast<const Store*>(store)->node(id);
    if (node == nullptr)
        return GRAPH_LITE_ERR_NOT_FOUND;
    *out_type = node->type;
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_store_edge_type(const graph_lite_store* store, graph_lite_edge_id id,
                                             graph_lite_edge_type* out_type) {
    if (store == nullptr || out_type == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const EdgeRecord* const edge = reinterpret_cast<const Store*>(store)->edge(id);
    if (edge == nullptr)
        return GRAPH_LITE_ERR_NOT_FOUND;
    *out_type = edge->type;
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_store_edge_endpoints(const graph_lite_store* store,
                                                  graph_lite_edge_id id,
                                                  graph_lite_node_id* out_from,
                                                  graph_lite_node_id* out_to) {
    if (store == nullptr || out_from == nullptr || out_to == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const EdgeRecord* const edge = reinterpret_cast<const Store*>(store)->edge(id);
    if (edge == nullptr)
        return GRAPH_LITE_ERR_NOT_FOUND;
    *out_from = edge->from;
    *out_to = edge->to;
    return GRAPH_LITE_OK;
}

} // extern "C"
