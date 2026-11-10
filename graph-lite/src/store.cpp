#include "store.hpp"

#include "graph-lite.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace graph_lite::internal {
namespace {

constexpr uint64_t kInitialRunCap = 4;

} // namespace

// ---------------------------------------------------------------------------
// SlotPool
// ---------------------------------------------------------------------------

uint64_t SlotPool::alloc(uint64_t cap) {
    // Append-only: holes are reclaimed by adjacent growth and by compaction,
    // never by a scan, so allocation stays O(1) while runs relocate freely.
    const uint64_t begin = static_cast<uint64_t>(cells_.size());
    cells_.resize(cells_.size() + static_cast<size_t>(cap));
    return begin;
}

void SlotPool::undo_alloc(uint64_t size) {
    assert(size <= cells_.size() && "undo_alloc past the pool end");
    cells_.resize(static_cast<size_t>(size));
}

void SlotPool::release(uint64_t begin, uint64_t cap) {
    if (cap == 0)
        return;

    // Merge with whichever neighbours are also free — one hash lookup per
    // side. Right first (the right gap's key would move), then left. The only
    // fallible step is the merged region's insert, and it runs before anything
    // is erased or counted: a throw leaves gaps_ and free_cells_ exactly as
    // they were, so the caller still owns the run and a retry re-releases it
    // once, never half-committed or twice-counted.
    uint64_t merged = cap;
    const auto right = gaps_.find(begin + cap);
    if (right != gaps_.end())
        merged += right->second;
    const auto left = gaps_.find(begin);
    if (left != gaps_.end()) {
        if (right != gaps_.end())
            gaps_.erase(right);
        left->second += merged;
    } else {
        gaps_.emplace(begin, merged); // strong guarantee: a throw changes nothing
        if (right != gaps_.end())
            gaps_.erase(begin + cap); // by key — the insert may have rehashed
    }
    free_cells_ += merged;
}

bool SlotPool::grow_in_place(Run& run, uint64_t new_cap) {
    const uint64_t need = new_cap - run.cap;
    const auto it = gaps_.find(run.begin + run.cap);
    if (it == gaps_.end() || it->second < need)
        return false;
    const uint64_t at = it->first;
    const uint64_t cap = it->second;
    // Insert the re-keyed remainder before erasing the absorbed gap: the
    // insert is the only fallible step, and on a throw nothing has changed.
    if (cap > need)
        gaps_.emplace(at + need, cap - need);
    gaps_.erase(at); // by key — the insert above may have rehashed
    free_cells_ -= need;
    run.cap = new_cap;
    return true;
}

uint64_t& SlotPool::cell(const Run& run, uint64_t pos) {
    return cells_[static_cast<size_t>(run.begin + pos)];
}

uint64_t SlotPool::cell(const Run& run, uint64_t pos) const {
    return cells_[static_cast<size_t>(run.begin + pos)];
}

void SlotPool::adopt_packed(std::vector<uint64_t>&& packed) {
    cells_ = std::move(packed);
    gaps_.clear();
    free_cells_ = 0;
}

uint64_t SlotPool::memory_bytes() const {
    return static_cast<uint64_t>(cells_.capacity()) * sizeof(uint64_t) +
           static_cast<uint64_t>(gaps_.bucket_count()) *
               static_cast<uint64_t>(sizeof(std::pair<uint64_t, uint64_t>));
}

// ---------------------------------------------------------------------------
// Store — nodes
// ---------------------------------------------------------------------------

graph_lite_status Store::node_add(uint64_t type, graph_lite_id* out_id) {
    assert(out_id != nullptr && "null out_id is rejected at the C seam");
    const graph_lite_id id = next_node_id_;
    node_index_.emplace(id, static_cast<uint64_t>(nodes_.size()));
    try {
        nodes_.push_back(NodeRec{id, type, Run{}, Run{}});
    } catch (...) {
        node_index_.erase(id); // roll the index back — no-throw
        throw;
    }
    next_node_id_ = id + 1; // the id is issued only once its record exists
    *out_id = id;
    return GRAPH_LITE_OK;
}

graph_lite_status Store::node_remove(graph_lite_id id) {
    const auto it = node_index_.find(id);
    if (it == node_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    const uint64_t slot = it->second;
    NodeRec& doomed = nodes_[static_cast<size_t>(slot)];

    // Drain incident edges first, always from the run's tail, so the dying edge
    // is its run's last entry and its own side of edge_remove_at is a no-op
    // swap. An edge can never outlive an endpoint (docs/graph-lite/data-model.md).
    while (doomed.out.len > 0)
        edge_remove_at(out_pool_.cell(doomed.out, doomed.out.len - 1));
    while (doomed.in.len > 0)
        edge_remove_at(in_pool_.cell(doomed.in, doomed.in.len - 1));

    // Each release commits alone: zeroing the run right after it succeeds
    // keeps a later throw from leaving a live record whose cells are already
    // on the free list — the shape where a retry re-releases the same cells
    // and another node's growth claims them. A zeroed run also never reaches
    // compaction with stale begin/cap to repack.
    if (doomed.out.cap > 0) {
        out_pool_.release(doomed.out.begin, doomed.out.cap);
        doomed.out = Run{};
    }
    if (doomed.in.cap > 0) {
        in_pool_.release(doomed.in.begin, doomed.in.cap);
        doomed.in = Run{};
    }
    maybe_compact(out_pool_, true);
    maybe_compact(in_pool_, false);

    node_index_.erase(id);

    const uint64_t back = static_cast<uint64_t>(nodes_.size() - 1);
    if (slot != back) {
        nodes_[static_cast<size_t>(slot)] = nodes_[static_cast<size_t>(back)];
        node_index_.at(nodes_[static_cast<size_t>(slot)].id) = slot;
    }
    nodes_.pop_back();
    return GRAPH_LITE_OK;
}

graph_lite_status Store::node_type(graph_lite_id id, uint64_t* out_type) const {
    assert(out_type != nullptr && "null out_type is rejected at the C seam");
    const auto it = node_index_.find(id);
    if (it == node_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    *out_type = nodes_[static_cast<size_t>(it->second)].type;
    return GRAPH_LITE_OK;
}

// ---------------------------------------------------------------------------
// Store — edges
// ---------------------------------------------------------------------------

graph_lite_status Store::edge_add(graph_lite_id from, graph_lite_id to, uint64_t type,
                                  graph_lite_id* out_id) {
    assert(out_id != nullptr && "null out_id is rejected at the C seam");
    const auto fit = node_index_.find(from);
    if (fit == node_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    const auto tit = node_index_.find(to);
    if (tit == node_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    NodeRec& fn = nodes_[static_cast<size_t>(fit->second)];
    NodeRec& tn = nodes_[static_cast<size_t>(tit->second)];

    // Grow before any state changes, so a failed allocation leaves the store
    // untouched. fn and tn may be the same record (self-loop); the two runs are
    // independent slices.
    if (graph_lite_status s = grow_run(out_pool_, fn.out, true))
        return s;
    if (graph_lite_status s = grow_run(in_pool_, tn.in, false))
        return s;

    EdgeRec rec;
    rec.id = next_edge_id_;
    rec.from = from;
    rec.to = to;
    rec.type = type;
    rec.out_pos = fn.out.len;
    rec.in_pos = tn.in.len;

    const uint64_t slot = static_cast<uint64_t>(edges_.size());
    edges_.push_back(rec);
    try {
        edge_index_.emplace(rec.id, slot);
    } catch (...) {
        edges_.pop_back(); // roll the orphan record back — no-throw
        throw;
    }

    out_pool_.cell(fn.out, rec.out_pos) = slot;
    fn.out.len += 1;
    in_pool_.cell(tn.in, rec.in_pos) = slot;
    tn.in.len += 1;

    next_edge_id_ = rec.id + 1; // issued only once both directions point at it
    *out_id = rec.id;
    return GRAPH_LITE_OK;
}

graph_lite_status Store::edge_remove(graph_lite_id id) {
    const auto it = edge_index_.find(id);
    if (it == edge_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    edge_remove_at(it->second);
    return GRAPH_LITE_OK;
}

graph_lite_status Store::edge_type(graph_lite_id id, uint64_t* out_type) const {
    assert(out_type != nullptr && "null out_type is rejected at the C seam");
    const auto it = edge_index_.find(id);
    if (it == edge_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    *out_type = edges_[static_cast<size_t>(it->second)].type;
    return GRAPH_LITE_OK;
}

void Store::edge_remove_at(uint64_t slot) {
    const EdgeRec rec = edges_[static_cast<size_t>(slot)];
    NodeRec& fn = nodes_[static_cast<size_t>(node_index_.at(rec.from))];
    NodeRec& tn = nodes_[static_cast<size_t>(node_index_.at(rec.to))];

    // Swap each run's last live cell into the vacated position; the edge that
    // moves keeps its position, so its record is rewritten to match. A run of
    // length 1 swaps the dying edge with itself and just shortens.
    const uint64_t out_moved = out_pool_.cell(fn.out, fn.out.len - 1);
    out_pool_.cell(fn.out, rec.out_pos) = out_moved;
    edges_[static_cast<size_t>(out_moved)].out_pos = rec.out_pos;
    fn.out.len -= 1;

    const uint64_t in_moved = in_pool_.cell(tn.in, tn.in.len - 1);
    in_pool_.cell(tn.in, rec.in_pos) = in_moved;
    edges_[static_cast<size_t>(in_moved)].in_pos = rec.in_pos;
    tn.in.len -= 1;

    // Dense swap-remove of the record itself: whichever edge moves into `slot`
    // changed index, and everything that points at it — its index-map entry and
    // both pools — must be re-pointed.
    const uint64_t back = static_cast<uint64_t>(edges_.size() - 1);
    if (slot != back) {
        NodeRec& mf =
            nodes_[static_cast<size_t>(node_index_.at(edges_[static_cast<size_t>(back)].from))];
        NodeRec& mt =
            nodes_[static_cast<size_t>(node_index_.at(edges_[static_cast<size_t>(back)].to))];
        edges_[static_cast<size_t>(slot)] = edges_[static_cast<size_t>(back)];
        edge_index_.at(edges_[static_cast<size_t>(slot)].id) = slot;
        out_pool_.cell(mf.out, edges_[static_cast<size_t>(slot)].out_pos) = slot;
        in_pool_.cell(mt.in, edges_[static_cast<size_t>(slot)].in_pos) = slot;
    }
    edges_.pop_back();
    edge_index_.erase(rec.id);
}

// ---------------------------------------------------------------------------
// Store — traversal queries
// ---------------------------------------------------------------------------

graph_lite_status Store::out_degree(graph_lite_id id, uint64_t* out_degree) const {
    assert(out_degree != nullptr && "null out_degree is rejected at the C seam");
    const auto it = node_index_.find(id);
    if (it == node_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    *out_degree = nodes_[static_cast<size_t>(it->second)].out.len;
    return GRAPH_LITE_OK;
}

graph_lite_status Store::in_degree(graph_lite_id id, uint64_t* out_degree) const {
    assert(out_degree != nullptr && "null out_degree is rejected at the C seam");
    const auto it = node_index_.find(id);
    if (it == node_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    *out_degree = nodes_[static_cast<size_t>(it->second)].in.len;
    return GRAPH_LITE_OK;
}

graph_lite_status Store::out_neighbors(graph_lite_id id, graph_lite_id* buffer, uint64_t capacity,
                                       uint64_t* out_count) const {
    assert(out_count != nullptr && "null out_count is rejected at the C seam");
    const auto it = node_index_.find(id);
    if (it == node_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    return copy_neighbors(nodes_[static_cast<size_t>(it->second)].out, out_pool_, true, buffer,
                          capacity, out_count);
}

graph_lite_status Store::in_neighbors(graph_lite_id id, graph_lite_id* buffer, uint64_t capacity,
                                      uint64_t* out_count) const {
    assert(out_count != nullptr && "null out_count is rejected at the C seam");
    const auto it = node_index_.find(id);
    if (it == node_index_.end())
        return GRAPH_LITE_ERR_NOT_FOUND;
    return copy_neighbors(nodes_[static_cast<size_t>(it->second)].in, in_pool_, false, buffer,
                          capacity, out_count);
}

graph_lite_status Store::copy_neighbors(const Run& run, const SlotPool& pool, bool out_side,
                                        graph_lite_id* buffer, uint64_t capacity,
                                        uint64_t* out_count) const {
    // Check order is the contract: capacity first (so buffer == NULL with
    // capacity 0 stays the "how many?" probe), then a null buffer that would
    // actually be written into.
    if (run.len > capacity) {
        *out_count = run.len;
        return GRAPH_LITE_ERR_BUFFER_TOO_SMALL;
    }
    if (run.len > 0 && buffer == nullptr)
        return GRAPH_LITE_ERR_INVALID_ARGUMENT;
    for (uint64_t i = 0; i < run.len; ++i) {
        const EdgeRec& edge = edges_[static_cast<size_t>(pool.cell(run, i))];
        buffer[i] = out_side ? edge.to : edge.from;
    }
    *out_count = run.len;
    return GRAPH_LITE_OK;
}

// ---------------------------------------------------------------------------
// Store — layout maintenance
// ---------------------------------------------------------------------------

graph_lite_status Store::grow_run(SlotPool& pool, Run& run, bool out_side) {
    if (run.len < run.cap)
        return GRAPH_LITE_OK;

    // Doubling; a node's first edge gets kInitialRunCap cells so the first
    // handful of edges do not each pay a relocation.
    const uint64_t new_cap = run.cap == 0 ? kInitialRunCap : run.cap * 2;

    if (!pool.grow_in_place(run, new_cap)) {
        const uint64_t fresh = pool.alloc(new_cap);
        std::memcpy(pool.data() + static_cast<size_t>(fresh),
                    pool.data() + static_cast<size_t>(run.begin),
                    static_cast<size_t>(run.len) * sizeof(uint64_t));
        if (run.cap > 0) {
            try {
                pool.release(run.begin, run.cap);
            } catch (...) {
                pool.undo_alloc(fresh); // the fresh cells belong to no one yet
                throw;
            }
        }
        run.begin = fresh;
        run.cap = new_cap;
    }
    maybe_compact(pool, out_side);
    return GRAPH_LITE_OK;
}

void Store::maybe_compact(SlotPool& pool, bool out_side) {
    if (pool.free_cells() * 2 <= pool.live_cells())
        return; // below half waste, the repack copy costs more than the space saves

    // Repack runs in current begin order; each keeps its capacity, only its
    // begin changes. Walking sorted positions makes the copy one forward sweep.
    std::vector<std::pair<uint64_t, uint64_t>> order; // <begin, node slot>
    order.reserve(nodes_.size());
    uint64_t packed_cells = 0;
    for (size_t s = 0; s < nodes_.size(); ++s) {
        const Run& run = out_side ? nodes_[s].out : nodes_[s].in;
        if (run.cap > 0) {
            order.emplace_back(run.begin, static_cast<uint64_t>(s));
            packed_cells += run.cap;
        }
    }
    std::sort(order.begin(), order.end());

    std::vector<uint64_t> packed(packed_cells);
    uint64_t cursor = 0;
    for (const auto& [begin, s] : order) {
        Run& run =
            out_side ? nodes_[static_cast<size_t>(s)].out : nodes_[static_cast<size_t>(s)].in;
        std::memcpy(packed.data() + static_cast<size_t>(cursor),
                    pool.data() + static_cast<size_t>(begin),
                    static_cast<size_t>(run.cap) * sizeof(uint64_t));
        run.begin = cursor;
        cursor += run.cap;
    }
    pool.adopt_packed(std::move(packed));
}

// ---------------------------------------------------------------------------
// Store — accounting
// ---------------------------------------------------------------------------

uint64_t Store::memory_usage() const {
    // Hash indices counted at bucket granularity; per-node allocator overhead
    // is ignored. A capacity estimate, not an account.
    const auto buckets = [](const std::unordered_map<graph_lite_id, uint64_t>& index) {
        return static_cast<uint64_t>(index.bucket_count()) *
               static_cast<uint64_t>(sizeof(std::pair<graph_lite_id, uint64_t>));
    };
    return static_cast<uint64_t>(nodes_.capacity()) * static_cast<uint64_t>(sizeof(NodeRec)) +
           static_cast<uint64_t>(edges_.capacity()) * static_cast<uint64_t>(sizeof(EdgeRec)) +
           out_pool_.memory_bytes() + in_pool_.memory_bytes() + buckets(node_index_) +
           buckets(edge_index_);
}

} // namespace graph_lite::internal
