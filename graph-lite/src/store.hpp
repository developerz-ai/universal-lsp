// Internal storage for graph-lite — private to src/, invisible to the C header.
//
// Shape (docs/graph-lite/data-model.md):
//   - Node and edge records live in dense vectors; removal swap-removes the
//     last record into the hole, so storage stays compact and iteration is
//     cache-friendly.
//   - Both adjacency directions are materialized. Each node owns a RUN of edge
//     slots in each of two flat pools: out_pool_ holds the edges it sources,
//     in_pool_ the edges that target it. A degree is a run length; a neighbor
//     list is a contiguous slice — there is no per-node allocation.
//   - Runs grow by doubling and relocate to a fresh gap when they cannot widen
//     in place. EdgeRec stores positions WITHIN a run, so a relocation rewrites
//     one begin instead of N records.
//   - Ids are issued monotonically and never reused, so id -> slot must go
//     through the index maps; slots themselves are dense and never stable.
//
// Single-writer by contract (docs/graph-lite/README.md): nothing here is
// thread-safe; the caller serializes.
#ifndef GRAPH_LITE_STORE_HPP
#define GRAPH_LITE_STORE_HPP

#include "graph-lite.h" // graph_lite_id, graph_lite_status — the ABI spellings

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace graph_lite::internal {

// Property values are one of the four ABI value kinds; keys are std::string.
// Tables are keyed by owner id in the owner's own id space. Defined here — not
// in model.cpp — because the handle owns the tables, and the handle is completed
// at the bottom of this file.
using PropertyValue = std::variant<int64_t, double, bool, std::string>;
using PropertyMap = std::unordered_map<std::string, PropertyValue>;
using PropertyTable = std::unordered_map<graph_lite_id, PropertyMap>;

// A node's slice of one pool: cells [begin, begin + cap), the first len live.
struct Run {
    uint64_t begin = 0;
    uint64_t len = 0;
    uint64_t cap = 0;
};

struct NodeRec {
    graph_lite_id id = 0;
    uint64_t type = 0;
    Run out;
    Run in;
};

// out_pos / in_pos are positions within the owning runs, not pool indices —
// relocating a run moves its cells without touching any EdgeRec.
struct EdgeRec {
    graph_lite_id id = 0;
    graph_lite_id from = 0;
    graph_lite_id to = 0;
    uint64_t type = 0;
    uint64_t out_pos = 0;
    uint64_t in_pos = 0;
};

// A flat pool of uint64 cells, carved into per-node runs. Free space is a
// begin-indexed gap map (begin -> cap), so all three hot operations —
// allocation, release-with-coalescing, widen-in-place — are O(1) hash lookups.
// That bound is what lets runs relocate freely at scale: a side gap VECTOR
// sorted by begin degrades quadratically once relocations outnumber
// compactions (measured: 500k-edge bulk insert slowed 60x as the list grew).
// Holes are therefore reclaimed by adjacent growth and by compaction, never by
// a first-fit scan.
class SlotPool {
public:
    // Reserve cap contiguous cells, appended at the end of the pool. Cells are
    // not cleared; the caller overwrites the live prefix before anyone can
    // read it.
    uint64_t alloc(uint64_t cap);

    // Give back a tail alloc() reserved but the caller never committed to —
    // the rollback half of a relocation whose release failed. No run or gap
    // references cells at or past `size`; shrinking never reallocates.
    void undo_alloc(uint64_t size);

    // Return a run's cells to the free pool, merging with whichever adjacent
    // regions are also free, so gaps stay hole-per-region, not hole-per-run.
    // Commits atomically: the only allocation — recording the merged region —
    // happens before anything is erased or counted, so a failed release leaves
    // the pool untouched and the caller still owning the run.
    void release(uint64_t begin, uint64_t cap);

    // Widen run to new_cap by absorbing the gap that starts immediately after
    // it. False means the caller must relocate (alloc, copy, release). The
    // absorb commits atomically: the remainder is re-keyed before the old gap
    // is erased, so a failed widen changes nothing.
    bool grow_in_place(Run& run, uint64_t new_cap);

    // Cell access by run position — the only way in or out of the pool.
    uint64_t& cell(const Run& run, uint64_t pos);
    uint64_t cell(const Run& run, uint64_t pos) const;

    // Free vs live cells. Compaction pays once free reaches half of live.
    uint64_t free_cells() const { return free_cells_; }

    uint64_t live_cells() const { return static_cast<uint64_t>(cells_.size()) - free_cells_; }

    uint64_t size() const { return static_cast<uint64_t>(cells_.size()); }

    // Defragmentation support: raw backing store for the copy-out pass, then
    // wholesale adoption of the repacked layout, sized to the live cell count
    // exactly — after adoption there are no gaps and every alloc appends.
    uint64_t* data() { return cells_.data(); }

    void adopt_packed(std::vector<uint64_t>&& packed);

    uint64_t memory_bytes() const;

private:
    std::vector<uint64_t> cells_;
    std::unordered_map<uint64_t, uint64_t> gaps_; // begin -> cap
    uint64_t free_cells_ = 0;
};

// The phase-1 store: typed nodes, typed directed edges, both directions.
class Store {
public:
    graph_lite_status node_add(uint64_t type, graph_lite_id* out_id);
    graph_lite_status node_remove(graph_lite_id id);
    graph_lite_status node_type(graph_lite_id id, uint64_t* out_type) const;

    graph_lite_status edge_add(graph_lite_id from, graph_lite_id to, uint64_t type,
                               graph_lite_id* out_id);
    graph_lite_status edge_remove(graph_lite_id id);

    // Endpoints and tag of one edge. Any out pointer may be null.
    graph_lite_status edge_info(graph_lite_id id, graph_lite_id* from, graph_lite_id* to,
                                uint64_t* out_type) const;

    // Edge-id listing, main's model contract (unlike the neighbor listing
    // below): *out_count is always the required size, cap below it fails
    // BUFFER_TOO_SMALL, buffer == NULL with cap == 0 is the size query, and
    // ids come back ascending. Does not allocate.
    graph_lite_status out_edge_ids(graph_lite_id id, graph_lite_id* buffer, uint64_t capacity,
                                   uint64_t* out_count) const;
    graph_lite_status in_edge_ids(graph_lite_id id, graph_lite_id* buffer, uint64_t capacity,
                                  uint64_t* out_count) const;

    // Existence probes for the seam layer (e.g. has_owner for properties).
    bool has_node(graph_lite_id id) const;
    bool has_edge(graph_lite_id id) const;

    graph_lite_status out_degree(graph_lite_id id, uint64_t* out_degree) const;
    graph_lite_status in_degree(graph_lite_id id, uint64_t* out_degree) const;
    graph_lite_status out_neighbors(graph_lite_id id, graph_lite_id* buffer, uint64_t capacity,
                                    uint64_t* out_count) const;
    graph_lite_status in_neighbors(graph_lite_id id, graph_lite_id* buffer, uint64_t capacity,
                                   uint64_t* out_count) const;

    uint64_t node_count() const { return static_cast<uint64_t>(nodes_.size()); }

    uint64_t edge_count() const { return static_cast<uint64_t>(edges_.size()); }

    uint64_t memory_usage() const;

    // Append every live id, in storage order (NOT sorted). May throw
    // std::bad_alloc; the caller guards. Inline: store.cpp is near the file cap.
    void collect_node_ids(std::vector<graph_lite_id>& out) const {
        out.reserve(out.size() + nodes_.size());
        for (const NodeRec& node : nodes_)
            out.push_back(node.id);
    }

    void collect_edge_ids(std::vector<graph_lite_id>& out) const {
        out.reserve(out.size() + edges_.size());
        for (const EdgeRec& edge : edges_)
            out.push_back(edge.id);
    }

private:
    // Widen a full run by doubling, in place when possible, by relocation when
    // not; out_side says which NodeRec field the pool's runs live in, so a
    // triggered compaction can rewrite them.
    graph_lite_status grow_run(SlotPool& pool, Run& run, bool out_side);

    // O(1) removal: swap-within-run in both pools via the stored positions,
    // then dense swap-remove of the record itself with pool fixups.
    void edge_remove_at(uint64_t slot);

    // Rebuild the pool once free space reaches half the live data — the point
    // where fragmentation, not data, drives the footprint. Relocation waste
    // from geometric doubling approaches live but rarely exceeds it, so a
    // literal "free > live" trigger almost never fires.
    void maybe_compact(SlotPool& pool, bool out_side);

    graph_lite_status copy_neighbors(const Run& run, const SlotPool& pool, bool out_side,
                                     graph_lite_id* buffer, uint64_t capacity,
                                     uint64_t* out_count) const;

    // Edge-id twin of copy_neighbors, with the model's contract instead.
    graph_lite_status copy_edge_ids(const Run& run, const SlotPool& pool, graph_lite_id* buffer,
                                    uint64_t capacity, uint64_t* out_count) const;

    std::vector<NodeRec> nodes_;                             // dense; slot == position
    std::vector<EdgeRec> edges_;                             // dense; slot == position
    std::unordered_map<graph_lite_id, uint64_t> node_index_; // id -> slot
    std::unordered_map<graph_lite_id, uint64_t> edge_index_; // id -> slot
    SlotPool out_pool_;
    SlotPool in_pool_;
    uint64_t next_node_id_ = 1; // 0 never names an entity
    uint64_t next_edge_id_ = 1;
};

} // namespace graph_lite::internal

// Completes the handle typedef'd in graph-lite.h. The C header deliberately
// leaves it opaque — no binding may size or depend on this layout. Adjacency
// lives in the store; property tables live beside it, keyed by owner id, so a
// walk of the adjacency never touches property storage
// (docs/graph-lite/data-model.md).
struct graph_lite_graph {
    graph_lite::internal::Store store;
    graph_lite::internal::PropertyTable node_properties;
    graph_lite::internal::PropertyTable edge_properties;

    graph_lite::internal::PropertyTable& properties(graph_lite_kind kind) {
        return kind == GRAPH_LITE_NODE ? node_properties : edge_properties;
    }

    const graph_lite::internal::PropertyTable& properties(graph_lite_kind kind) const {
        return kind == GRAPH_LITE_NODE ? node_properties : edge_properties;
    }

    bool has_owner(graph_lite_kind kind, graph_lite_id id) const {
        return kind == GRAPH_LITE_NODE ? store.has_node(id) : store.has_edge(id);
    }
};

#endif // GRAPH_LITE_STORE_HPP
