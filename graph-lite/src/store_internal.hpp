// graph-lite — internal model shared by store.cpp and cursor.cpp.
//
// Not exported and not installed: nothing outside src/ includes it, and no
// name in it may appear in the public header. Ids are stable and never
// reused; the slot vectors underneath recycle slots freely — the id->slot
// maps are the only stable address of a record.
#ifndef GRAPH_LITE_SRC_STORE_INTERNAL_HPP
#define GRAPH_LITE_SRC_STORE_INTERNAL_HPP

#include "graph-lite.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace graph_lite::internal {

// The NONE macros of the C header, spelled for C++: the macros carry a
// C-style cast, which -Wold-style-cast rejects at every C++ expansion.
inline constexpr graph_lite_edge_id kNoEdge = 0;

struct NodeRecord {
    graph_lite_node_id id;
    graph_lite_node_type type;
    std::vector<graph_lite_edge_id> out_edges; // insertion order
    std::vector<graph_lite_edge_id> in_edges;  // insertion order
};

struct EdgeRecord {
    graph_lite_edge_id id;
    graph_lite_node_id from;
    graph_lite_node_id to;
    graph_lite_edge_type type;
};

/// One node of a completed walk, fixed at cursor_open time: which node, how
/// deep in the walk tree, and the edge (and node) the walk arrived through.
struct Step {
    graph_lite_node_id node;
    uint64_t depth;
    graph_lite_edge_id edge; // GRAPH_LITE_EDGE_ID_NONE for the start node
    graph_lite_node_id from; // == node for the start node
};

struct Store {
    std::vector<NodeRecord> nodes; // slot storage; slots recycle, ids never do
    std::vector<EdgeRecord> edges;
    std::unordered_map<graph_lite_node_id, size_t> node_slot;
    std::unordered_map<graph_lite_edge_id, size_t> edge_slot;
    std::vector<size_t> free_node_slots;
    std::vector<size_t> free_edge_slots;
    uint64_t node_count = 0;
    uint64_t edge_count = 0;
    graph_lite_node_id next_node_id = 1; // monotonic: a number never comes back
    graph_lite_edge_id next_edge_id = 1;

    NodeRecord* node(graph_lite_node_id id);
    EdgeRecord* edge(graph_lite_edge_id id);
    const NodeRecord* node(graph_lite_node_id id) const;
    const EdgeRecord* edge(graph_lite_edge_id id) const;
};

/// A finished walk and how far a caller has read into it. Holds only values,
/// so a cursor stays valid no matter what happens to the store afterwards.
struct Cursor {
    std::vector<Step> steps; // never empty: the start node always emits
    size_t position = 0;     // index of the step cursor_next returns next
    bool truncated = false;
};

/// The step the cursor's accessors describe: the node cursor_next returned
/// last — before the first call, the start node, per the C contract.
inline const Step& current(const Cursor& cursor) {
    return cursor.steps[cursor.position == 0 ? 0 : cursor.position - 1];
}

} // namespace graph_lite::internal

#endif // GRAPH_LITE_SRC_STORE_INTERNAL_HPP
