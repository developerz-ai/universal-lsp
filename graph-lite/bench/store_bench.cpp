// Deterministic benchmark of the public C API — the numbers behind
// docs/graph-lite/benchmark.md. Fixed seed, fixed iteration counts, wall-clock
// timings only: this prints measurements, it asserts nothing about them, so a
// slow box changes the numbers and never the exit status.
//
// Build and run (release preset, RelWithDebInfo):
//   cmake --preset release && cmake --build --preset release --target graph_lite_bench
//   ./build/release/graph-lite/graph_lite_bench
//
// Calls the C functions directly, in the shape an FFI binding would: degree
// query, then a caller-owned buffer copy. No timing assertions anywhere.
#include "graph-lite.h"

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

namespace {

constexpr uint64_t kNodes = 100000;
constexpr uint64_t kEdges = 500000;
constexpr uint64_t kWalkRepeats = 3;
constexpr uint64_t kSeed = 12345;

using Clock = std::chrono::steady_clock;

// Accumulated and printed so the walk loops cannot be optimized away as dead
// work; the value itself carries no meaning.
uint64_t g_sink = 0;

struct Row {
    const char* name;
    uint64_t ops;
    double ms;
    double ns_per_op;
    double bytes_per_node;
    double bytes_per_edge;
};

double elapsed_ms(Clock::time_point start) {
    const auto span = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start);
    return static_cast<double>(span.count()) / 1e6;
}

void must_ok(graph_lite_status status, const char* what) {
    if (status != GRAPH_LITE_OK) {
        std::printf("bench aborted: %s returned %s\n", what, graph_lite_status_string(status));
        std::abort();
    }
}

Row make_row(const char* name, uint64_t ops, double ms, uint64_t bytes, uint64_t nodes,
             uint64_t edges) {
    Row row;
    row.name = name;
    row.ops = ops;
    row.ms = ms;
    row.ns_per_op = ops > 0 ? ms * 1e6 / static_cast<double>(ops) : -1.0;
    row.bytes_per_node = nodes > 0 ? static_cast<double>(bytes) / static_cast<double>(nodes) : -1.0;
    row.bytes_per_edge = edges > 0 ? static_cast<double>(bytes) / static_cast<double>(edges) : -1.0;
    return row;
}

void print_row(const Row& row) {
    std::printf("%-14s %11" PRIu64 " ", row.name, row.ops);
    if (row.ops > 0)
        std::printf("%10.1f %12.0f ", row.ms, row.ns_per_op);
    else
        std::printf("%10s %12s ", "--", "--");
    if (row.bytes_per_node >= 0.0)
        std::printf("%12.1f %12.1f\n", row.bytes_per_node, row.bytes_per_edge);
    else
        std::printf("%12s %12s\n", "--", "--");
}

graph_lite_id pick(const std::vector<graph_lite_id>& live, std::mt19937_64& rng) {
    return live[static_cast<size_t>(rng() % live.size())];
}

} // namespace

int main() {
    std::mt19937_64 rng(kSeed);
    std::vector<Row> rows;
    rows.reserve(5);

    graph_lite_graph* g = graph_lite_graph_create();
    if (g == nullptr) {
        std::printf("bench aborted: create returned NULL\n");
        return 1;
    }
    std::vector<graph_lite_id> live;
    live.reserve(static_cast<size_t>(kNodes));

    // -- bulk insert: the cold-start cost every embedder pays first ----------
    double ms = 0.0;
    {
        const auto t0 = Clock::now();
        for (uint64_t i = 0; i < kNodes; ++i) {
            graph_lite_id id = 0;
            must_ok(graph_lite_node_add(g, rng() % 16, &id), "node_add");
            live.push_back(id);
        }
        for (uint64_t e = 0; e < kEdges; ++e) {
            graph_lite_id id = 0;
            must_ok(graph_lite_edge_add(g, pick(live, rng), pick(live, rng), rng() % 16, &id),
                    "edge_add");
        }
        ms = elapsed_ms(t0);
        uint64_t bytes = 0;
        must_ok(graph_lite_graph_memory_usage(g, &bytes), "memory_usage");
        rows.push_back(make_row("bulk_insert", kNodes + kEdges, ms, bytes, kNodes, kEdges));
        g_sink += bytes;
    }

    // -- full forward walk: degree + neighbor copy for every node ------------
    {
        std::vector<graph_lite_id> buf;
        const auto t0 = Clock::now();
        for (uint64_t rep = 0; rep < kWalkRepeats; ++rep) {
            for (const graph_lite_id id : live) {
                uint64_t deg = 0;
                must_ok(graph_lite_node_out_degree(g, id, &deg), "out_degree");
                if (deg > buf.size())
                    buf.resize(static_cast<size_t>(deg));
                if (deg > 0) {
                    uint64_t count = 0;
                    must_ok(graph_lite_node_out_neighbors(g, id, buf.data(), deg, &count),
                            "out_neighbors");
                    for (uint64_t i = 0; i < count; ++i)
                        g_sink += buf[i];
                }
            }
        }
        rows.push_back(make_row("forward_walk", kNodes * kWalkRepeats, elapsed_ms(t0), 0, 0, 0));
    }

    // -- full reverse walk: the direction that must not be second-class ------
    {
        std::vector<graph_lite_id> buf;
        const auto t0 = Clock::now();
        for (uint64_t rep = 0; rep < kWalkRepeats; ++rep) {
            for (const graph_lite_id id : live) {
                uint64_t deg = 0;
                must_ok(graph_lite_node_in_degree(g, id, &deg), "in_degree");
                if (deg > buf.size())
                    buf.resize(static_cast<size_t>(deg));
                if (deg > 0) {
                    uint64_t count = 0;
                    must_ok(graph_lite_node_in_neighbors(g, id, buf.data(), deg, &count),
                            "in_neighbors");
                    for (uint64_t i = 0; i < count; ++i)
                        g_sink += buf[i];
                }
            }
        }
        rows.push_back(make_row("reverse_walk", kNodes * kWalkRepeats, elapsed_ms(t0), 0, 0, 0));
    }

    // -- remove/re-add churn: half the nodes cascade away, then come back ----
    {
        const auto t0 = Clock::now();
        uint64_t ops = 0;
        // Descending indices: a swap-remove never disturbs an index below the
        // cursor, so every second entry goes without reshuffling the rest.
        for (size_t i = live.size(); i >= 2; i -= 2) {
            const graph_lite_id id = live[i - 2];
            must_ok(graph_lite_node_remove(g, id), "node_remove");
            live[i - 2] = live.back();
            live.pop_back();
            ++ops;
        }
        for (uint64_t i = 0; i < kNodes / 2; ++i) {
            graph_lite_id id = 0;
            must_ok(graph_lite_node_add(g, rng() % 16, &id), "node_add");
            live.push_back(id);
            ++ops;
        }
        for (uint64_t e = 0; e < kEdges / 2; ++e) {
            graph_lite_id id = 0;
            must_ok(graph_lite_edge_add(g, pick(live, rng), pick(live, rng), rng() % 16, &id),
                    "edge_add");
            ++ops;
        }
        rows.push_back(make_row("churn", ops, elapsed_ms(t0), 0, 0, 0));
    }

    // -- storage footprint after the churn (fragmentation included) ----------
    {
        uint64_t bytes = 0;
        uint64_t nodes = 0;
        uint64_t edges = 0;
        must_ok(graph_lite_graph_memory_usage(g, &bytes), "memory_usage");
        must_ok(graph_lite_graph_node_count(g, &nodes), "node_count");
        must_ok(graph_lite_graph_edge_count(g, &edges), "edge_count");
        rows.push_back(make_row("footprint", 0, 0.0, bytes, nodes, edges));
        g_sink += bytes;
    }

    std::printf("graph-lite store benchmark (seed %" PRIu64 ", %" PRIu64 " walk repeats)\n", kSeed,
                kWalkRepeats);
    std::printf("%-14s %11s %10s %12s %12s %12s\n", "scenario", "ops", "ms", "ns/op", "bytes/node",
                "bytes/edge");
    for (const Row& row : rows)
        print_row(row);
    std::printf("checksum %" PRIu64 " (guard against dead-code elimination)\n", g_sink);

    graph_lite_graph_destroy(g);
    return 0;
}
