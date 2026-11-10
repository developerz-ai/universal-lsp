// The node/edge model behind the C ABI in graph-lite.h.
//
// This file owns the C seam for the model surface — create/destroy, add/remove,
// tags, edge info, counts, edge-id listing — and the whole property store. The
// adjacency itself lives in the store (src/store.cpp): per-node contiguous
// forward and reverse runs, with stable ids issued from two separate monotonic
// counters that start at 1 and only go up, so an id is never reused after
// removal and a caller's id is either the thing it always named or NOT_FOUND.
//
// Identity — unknown id, missing endpoint — is decided by the store, next to
// the indexes it owns; this seam validates only what the store cannot see: the
// null handle, the null out-parameter, the property kind. One seam per
// invariant keeps "where would this be checked" answerable.
//
// Layout: adjacency lives in the store; properties live in their own tables
// keyed by owner id, so a walk of the adjacency never touches property storage.
//
// Tags are opaque 64-bit values. They are stored and compared by == in callers'
// code; nothing here inspects, registers or validates one.

#include "graph-lite.h"

#include "store.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

using graph_lite::internal::PropertyMap;
using graph_lite::internal::PropertyTable;
using graph_lite::internal::PropertyValue;

bool valid_kind(graph_lite_kind kind) {
    return kind == GRAPH_LITE_NODE || kind == GRAPH_LITE_EDGE;
}

// Property keys enter through the C ABI as C strings. Compare them directly so
// reads and removal need no temporary std::string allocation.
auto find_key(const PropertyMap& properties, const char* key) {
    return std::find_if(properties.begin(), properties.end(),
                        [key](const auto& property) { return property.first == key; });
}

// Runs fn, turning a failure into the status the ABI promises. No exception may
// cross an extern "C" boundary, and allocation failure is not the only one the
// store can raise (a size limit surfaces as length_error) — anything escaping
// the call maps to OOM, the one status an embedder can act on.
template <typename Fn>
graph_lite_status guarded(Fn&& fn) {
    try {
        return fn();
    } catch (...) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    }
}

graph_lite_status set_property(graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                               const char* key, const char* string_value, PropertyValue value) {
    if (!g || !key || !valid_kind(kind))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    if (value.index() == 3) {
        if (!string_value)
            return GRAPH_LITE_ERR_INVALID_ARGUMENT;
        value = std::string(string_value);
    }
    if (!g->has_owner(kind, id))
        return GRAPH_LITE_ERR_NOT_FOUND;
    g->properties(kind)[id].insert_or_assign(key, std::move(value));
    return GRAPH_LITE_OK;
}

// Looks up a property. Sets *status and returns null unless it was found.
const PropertyValue* find_property(const graph_lite_graph* g, graph_lite_kind kind,
                                   graph_lite_id id, const char* key, const void* out,
                                   graph_lite_status* status) {
    if (!g || !key || !out || !valid_kind(kind)) {
        *status = GRAPH_LITE_ERR_INVALID_ARGUMENT;
        return nullptr;
    }
    const PropertyTable& table = g->properties(kind);
    const auto owner = table.find(id);
    if (owner == table.end()) {
        *status = GRAPH_LITE_ERR_NOT_FOUND;
        return nullptr;
    }
    const auto prop = find_key(owner->second, key);
    if (prop == owner->second.end()) {
        *status = GRAPH_LITE_ERR_NOT_FOUND;
        return nullptr;
    }
    *status = GRAPH_LITE_OK;
    return &prop->second;
}

template <typename T>
graph_lite_status get_typed(const graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                            const char* key, T* out) {
    graph_lite_status status;
    const PropertyValue* value = find_property(g, kind, id, key, out, &status);
    if (!value)
        return status;
    const T* held = std::get_if<T>(value);
    if (!held)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    *out = *held;
    return GRAPH_LITE_OK;
}

} // namespace

extern "C" {

graph_lite_status graph_lite_graph_create(graph_lite_graph** out) {
    if (!out)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] {
        *out = new graph_lite_graph();
        return GRAPH_LITE_OK;
    });
}

void graph_lite_graph_destroy(graph_lite_graph* g) {
    delete g;
}

graph_lite_status graph_lite_add_node(graph_lite_graph* g, graph_lite_tag tag,
                                      graph_lite_id* out_id) {
    if (!g || !out_id)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] { return g->store.node_add(tag, out_id); });
}

graph_lite_status graph_lite_remove_node(graph_lite_graph* g, graph_lite_id id) {
    if (!g)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] {
        // Collect the incident edge ids BEFORE anything changes: this gather is
        // the only step here that can fail, so a refused removal changes nothing
        // at all. The property erases cannot throw, and the store's drain is
        // allocation-free and commits atomically — a compaction refusal after
        // the drain leaves the post-removal state, never a third thing.
        uint64_t out_count = 0;
        uint64_t in_count = 0;
        graph_lite_status st = g->store.out_edge_ids(id, nullptr, 0, &out_count);
        if (st != GRAPH_LITE_OK && st != GRAPH_LITE_ERR_BUFFER_TOO_SMALL)
            return st;
        st = g->store.in_edge_ids(id, nullptr, 0, &in_count);
        if (st != GRAPH_LITE_OK && st != GRAPH_LITE_ERR_BUFFER_TOO_SMALL)
            return st;

        std::vector<graph_lite_id> incident(static_cast<size_t>(out_count + in_count));
        if (out_count > 0)
            g->store.out_edge_ids(id, incident.data(), out_count, &out_count);
        if (in_count > 0)
            g->store.in_edge_ids(id, incident.data() + out_count, in_count, &in_count);
        for (const graph_lite_id edge : incident)
            g->edge_properties.erase(edge); // a self-loop appears twice; erase is idempotent
        g->node_properties.erase(id);
        return g->store.node_remove(id);
    });
}

graph_lite_status graph_lite_node_tag(const graph_lite_graph* g, graph_lite_id id,
                                      graph_lite_tag* out_tag) {
    if (!g || !out_tag)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return g->store.node_type(id, out_tag);
}

graph_lite_status graph_lite_add_edge(graph_lite_graph* g, graph_lite_id from, graph_lite_id to,
                                      graph_lite_tag tag, graph_lite_id* out_id) {
    if (!g || !out_id)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] { return g->store.edge_add(from, to, tag, out_id); });
}

graph_lite_status graph_lite_remove_edge(graph_lite_graph* g, graph_lite_id id) {
    if (!g)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    if (!g->store.has_edge(id))
        return GRAPH_LITE_ERR_NOT_FOUND;
    g->edge_properties.erase(id); // cannot throw; a dead edge keeps no properties
    return g->store.edge_remove(id);
}

graph_lite_status graph_lite_edge_info(const graph_lite_graph* g, graph_lite_id id,
                                       graph_lite_id* from, graph_lite_id* to,
                                       graph_lite_tag* tag) {
    if (!g)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return g->store.edge_info(id, from, to, tag);
}

size_t graph_lite_node_count(const graph_lite_graph* g) {
    return g ? g->store.node_count() : 0;
}

size_t graph_lite_edge_count(const graph_lite_graph* g) {
    return g ? g->store.edge_count() : 0;
}

graph_lite_status graph_lite_out_edges(const graph_lite_graph* g, graph_lite_id node,
                                       graph_lite_id* buf, size_t cap, size_t* count) {
    if (!g || !count || (!buf && cap != 0))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return g->store.out_edge_ids(node, buf, cap, count);
}

graph_lite_status graph_lite_in_edges(const graph_lite_graph* g, graph_lite_id node,
                                      graph_lite_id* buf, size_t cap, size_t* count) {
    if (!g || !count || (!buf && cap != 0))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return g->store.in_edge_ids(node, buf, cap, count);
}

graph_lite_status graph_lite_set_int(graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                                     const char* key, int64_t value) {
    return guarded([&] { return set_property(g, kind, id, key, nullptr, value); });
}

graph_lite_status graph_lite_set_double(graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                                        const char* key, double value) {
    return guarded([&] { return set_property(g, kind, id, key, nullptr, value); });
}

graph_lite_status graph_lite_set_bool(graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                                      const char* key, int value) {
    return guarded([&] { return set_property(g, kind, id, key, nullptr, value != 0); });
}

graph_lite_status graph_lite_set_string(graph_lite_graph* g, graph_lite_kind kind, graph_lite_id id,
                                        const char* key, const char* value) {
    return guarded(
        [&] { return set_property(g, kind, id, key, value, PropertyValue(std::string())); });
}

graph_lite_status graph_lite_property_kind(const graph_lite_graph* g, graph_lite_kind kind,
                                           graph_lite_id id, const char* key,
                                           graph_lite_value_kind* out) {
    return guarded([&] {
        graph_lite_status status;
        const PropertyValue* value = find_property(g, kind, id, key, out, &status);
        if (!value)
            return status;
        // The variant alternatives are declared in graph_lite_value_kind order.
        *out = static_cast<graph_lite_value_kind>(value->index());
        return GRAPH_LITE_OK;
    });
}

graph_lite_status graph_lite_get_int(const graph_lite_graph* g, graph_lite_kind kind,
                                     graph_lite_id id, const char* key, int64_t* out) {
    return guarded([&] { return get_typed(g, kind, id, key, out); });
}

graph_lite_status graph_lite_get_double(const graph_lite_graph* g, graph_lite_kind kind,
                                        graph_lite_id id, const char* key, double* out) {
    return guarded([&] { return get_typed(g, kind, id, key, out); });
}

graph_lite_status graph_lite_get_bool(const graph_lite_graph* g, graph_lite_kind kind,
                                      graph_lite_id id, const char* key, int* out) {
    return guarded([&] {
        bool value = false;
        if (!out)
            return GRAPH_LITE_ERR_INVALID_ARGUMENT;
        const graph_lite_status status = get_typed(g, kind, id, key, &value);
        if (status == GRAPH_LITE_OK)
            *out = value ? 1 : 0;
        return status;
    });
}

graph_lite_status graph_lite_get_string(const graph_lite_graph* g, graph_lite_kind kind,
                                        graph_lite_id id, const char* key, char* buf, size_t cap,
                                        size_t* len) {
    return guarded([&] {
        graph_lite_status status;
        const PropertyValue* value = find_property(g, kind, id, key, len, &status);
        if (!value)
            return status;
        const std::string* text = std::get_if<std::string>(value);
        if (!text || (!buf && cap != 0))
            return GRAPH_LITE_ERR_INVALID_ARGUMENT;
        *len = text->size();
        if (cap < text->size() + 1)
            return GRAPH_LITE_ERR_BUFFER_TOO_SMALL;
        std::copy(text->begin(), text->end(), buf);
        buf[text->size()] = '\0';
        return GRAPH_LITE_OK;
    });
}

graph_lite_status graph_lite_remove_property(graph_lite_graph* g, graph_lite_kind kind,
                                             graph_lite_id id, const char* key) {
    if (!g || !key || !valid_kind(kind))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] {
        PropertyTable& table = g->properties(kind);
        const auto owner = table.find(id);
        if (owner == table.end())
            return GRAPH_LITE_ERR_NOT_FOUND;
        const auto property = find_key(owner->second, key);
        if (property == owner->second.end())
            return GRAPH_LITE_ERR_NOT_FOUND;
        owner->second.erase(property);
        if (owner->second.empty())
            table.erase(owner);
        return GRAPH_LITE_OK;
    });
}

} // extern "C"
