// The node/edge model behind the C ABI in graph-lite.h.
//
// Identity: node ids and edge ids come from two separate counters that start at 1
// and only go up. An id is never reused after removal, so a caller's id is either
// the thing it always named or NOT_FOUND. Keeping the counters apart from the
// containers is what makes that true — the counter, not "size + 1", issues ids.
//
// Layout: adjacency (per-node out/in edge-id vectors) lives in the node and edge
// tables; properties live in their own maps keyed by owner id. Walking edges never
// touches property storage.
//
// Tags are opaque 64-bit values. They are stored and compared by == in callers'
// code; nothing here inspects, registers or validates one.

#include "graph-lite.h"

#include <algorithm>
#include <new>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace {

using PropertyValue = std::variant<int64_t, double, bool, std::string>;
using PropertyMap = std::unordered_map<std::string, PropertyValue>;
using PropertyTable = std::unordered_map<graph_lite_id, PropertyMap>;

struct NodeRecord {
    graph_lite_tag tag;
    std::vector<graph_lite_id> out; // edge ids, ascending
    std::vector<graph_lite_id> in;  // edge ids, ascending
};

struct EdgeRecord {
    graph_lite_id from;
    graph_lite_id to;
    graph_lite_tag tag;
};

// Makes room for one more element so the following push_back cannot throw. Growth
// is geometric so a run of adds stays amortised O(1).
void reserve_one_more(std::vector<graph_lite_id>& ids) {
    if (ids.size() == ids.capacity())
        ids.reserve(std::max<std::size_t>(4, ids.capacity() * 2));
}

// Edge ids are issued in ascending order and appended, so each vector is already
// sorted; erase keeps it so.
void unlink(std::vector<graph_lite_id>& ids, graph_lite_id edge) {
    const auto it = std::lower_bound(ids.begin(), ids.end(), edge);
    if (it != ids.end() && *it == edge)
        ids.erase(it);
}

bool valid_kind(graph_lite_kind kind) {
    return kind == GRAPH_LITE_NODE || kind == GRAPH_LITE_EDGE;
}

// Runs fn, turning an allocation failure into the status the ABI promises. No
// exception may cross an extern "C" boundary.
template <typename Fn>
graph_lite_status guarded(Fn&& fn) {
    try {
        return fn();
    } catch (const std::bad_alloc&) {
        return GRAPH_LITE_ERR_OUT_OF_MEMORY;
    }
}

} // namespace

struct graph_lite_graph {
    std::unordered_map<graph_lite_id, NodeRecord> nodes;
    std::unordered_map<graph_lite_id, EdgeRecord> edges;
    PropertyTable node_properties;
    PropertyTable edge_properties;
    graph_lite_id next_node_id = 1;
    graph_lite_id next_edge_id = 1;

    PropertyTable& properties(graph_lite_kind kind) {
        return kind == GRAPH_LITE_NODE ? node_properties : edge_properties;
    }

    const PropertyTable& properties(graph_lite_kind kind) const {
        return kind == GRAPH_LITE_NODE ? node_properties : edge_properties;
    }

    bool has_owner(graph_lite_kind kind, graph_lite_id id) const {
        return kind == GRAPH_LITE_NODE ? nodes.count(id) != 0 : edges.count(id) != 0;
    }

    // Unlinks the edge from both endpoints, then drops it and its properties. The
    // caller has already checked the edge exists.
    void erase_edge(graph_lite_id id) {
        const EdgeRecord edge = edges.at(id);
        unlink(nodes.at(edge.from).out, id);
        unlink(nodes.at(edge.to).in, id);
        edge_properties.erase(id);
        edges.erase(id);
    }
};

namespace {

graph_lite_status list_edges(const graph_lite_graph* g, graph_lite_id node, bool outgoing,
                             graph_lite_id* buf, std::size_t cap, std::size_t* count) {
    if (!g || !count || (!buf && cap != 0))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const auto it = g->nodes.find(node);
    if (it == g->nodes.end())
        return GRAPH_LITE_ERR_NOT_FOUND;

    const std::vector<graph_lite_id>& ids = outgoing ? it->second.out : it->second.in;
    *count = ids.size();
    if (cap < ids.size())
        return GRAPH_LITE_ERR_BUFFER_TOO_SMALL;
    std::copy(ids.begin(), ids.end(), buf);
    return GRAPH_LITE_OK;
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
    const auto prop = owner->second.find(key);
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
    return guarded([&] {
        const graph_lite_id id = g->next_node_id;
        g->nodes.emplace(id, NodeRecord{tag, {}, {}});
        g->next_node_id = id + 1; // only once the node exists
        *out_id = id;
        return GRAPH_LITE_OK;
    });
}

graph_lite_status graph_lite_remove_node(graph_lite_graph* g, graph_lite_id id) {
    if (!g)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] {
        const auto it = g->nodes.find(id);
        if (it == g->nodes.end())
            return GRAPH_LITE_ERR_NOT_FOUND;

        // Copy first: erase_edge edits the very vectors being walked. A self-loop is
        // in both lists, so the second visit finds it already gone.
        std::vector<graph_lite_id> incident = it->second.out;
        incident.insert(incident.end(), it->second.in.begin(), it->second.in.end());
        for (const graph_lite_id edge : incident) {
            if (g->edges.count(edge) != 0)
                g->erase_edge(edge);
        }
        g->node_properties.erase(id);
        g->nodes.erase(id);
        return GRAPH_LITE_OK;
    });
}

graph_lite_status graph_lite_node_tag(const graph_lite_graph* g, graph_lite_id id,
                                      graph_lite_tag* out_tag) {
    if (!g || !out_tag)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const auto it = g->nodes.find(id);
    if (it == g->nodes.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    *out_tag = it->second.tag;
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_add_edge(graph_lite_graph* g, graph_lite_id from, graph_lite_id to,
                                      graph_lite_tag tag, graph_lite_id* out_id) {
    if (!g || !out_id)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    return guarded([&] {
        const auto from_it = g->nodes.find(from);
        const auto to_it = g->nodes.find(to);
        if (from_it == g->nodes.end() || to_it == g->nodes.end())
            return GRAPH_LITE_ERR_NOT_FOUND;

        // Everything that can throw happens before anything is linked, so a failed
        // add leaves the graph exactly as it was.
        reserve_one_more(from_it->second.out);
        reserve_one_more(to_it->second.in);
        const graph_lite_id id = g->next_edge_id;
        g->edges.emplace(id, EdgeRecord{from, to, tag});
        g->next_edge_id = id + 1;
        from_it->second.out.push_back(id);
        to_it->second.in.push_back(id);
        *out_id = id;
        return GRAPH_LITE_OK;
    });
}

graph_lite_status graph_lite_remove_edge(graph_lite_graph* g, graph_lite_id id) {
    if (!g)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    if (g->edges.count(id) == 0)
        return GRAPH_LITE_ERR_NOT_FOUND;
    g->erase_edge(id);
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_edge_info(const graph_lite_graph* g, graph_lite_id id,
                                       graph_lite_id* from, graph_lite_id* to,
                                       graph_lite_tag* tag) {
    if (!g)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const auto it = g->edges.find(id);
    if (it == g->edges.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    if (from)
        *from = it->second.from;
    if (to)
        *to = it->second.to;
    if (tag)
        *tag = it->second.tag;
    return GRAPH_LITE_OK;
}

size_t graph_lite_node_count(const graph_lite_graph* g) {
    return g ? g->nodes.size() : 0;
}

size_t graph_lite_edge_count(const graph_lite_graph* g) {
    return g ? g->edges.size() : 0;
}

graph_lite_status graph_lite_out_edges(const graph_lite_graph* g, graph_lite_id node,
                                       graph_lite_id* buf, size_t cap, size_t* count) {
    return list_edges(g, node, true, buf, cap, count);
}

graph_lite_status graph_lite_in_edges(const graph_lite_graph* g, graph_lite_id node,
                                      graph_lite_id* buf, size_t cap, size_t* count) {
    return list_edges(g, node, false, buf, cap, count);
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
    graph_lite_status status;
    const PropertyValue* value = find_property(g, kind, id, key, out, &status);
    if (!value)
        return status;
    // The variant alternatives are declared in graph_lite_value_kind order.
    *out = static_cast<graph_lite_value_kind>(value->index());
    return GRAPH_LITE_OK;
}

graph_lite_status graph_lite_get_int(const graph_lite_graph* g, graph_lite_kind kind,
                                     graph_lite_id id, const char* key, int64_t* out) {
    return get_typed(g, kind, id, key, out);
}

graph_lite_status graph_lite_get_double(const graph_lite_graph* g, graph_lite_kind kind,
                                        graph_lite_id id, const char* key, double* out) {
    return get_typed(g, kind, id, key, out);
}

graph_lite_status graph_lite_get_bool(const graph_lite_graph* g, graph_lite_kind kind,
                                      graph_lite_id id, const char* key, int* out) {
    bool value = false;
    if (!out)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    const graph_lite_status status = get_typed(g, kind, id, key, &value);
    if (status == GRAPH_LITE_OK)
        *out = value ? 1 : 0;
    return status;
}

graph_lite_status graph_lite_get_string(const graph_lite_graph* g, graph_lite_kind kind,
                                        graph_lite_id id, const char* key, char* buf, size_t cap,
                                        size_t* len) {
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
}

graph_lite_status graph_lite_remove_property(graph_lite_graph* g, graph_lite_kind kind,
                                             graph_lite_id id, const char* key) {
    if (!g || !key || !valid_kind(kind))
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    PropertyTable& table = g->properties(kind);
    const auto owner = table.find(id);
    if (owner == table.end() || owner->second.erase(key) == 0)
        return GRAPH_LITE_ERR_NOT_FOUND;
    if (owner->second.empty())
        table.erase(owner);
    return GRAPH_LITE_OK;
}

} // extern "C"
