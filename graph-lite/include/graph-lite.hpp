// graph-lite — C++ ergonomics over the C ABI.
//
// Header-only, and implemented ENTIRELY in terms of graph-lite.h. It exports no
// entry point of its own, so a C++ caller pays nothing for it and no C++ type
// ever reaches the linkable surface. The direction matters: the C header is the
// contract, this is a convenience — never the reverse.
//
// Including this instead of the C header buys RAII handles, real types, and
// exceptions where a code would otherwise be checked by hand. Callers binding
// from another language ignore it.
#ifndef GRAPH_LITE_HPP
#define GRAPH_LITE_HPP

#include "graph-lite.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace graph_lite {

/// Version of the library the caller actually linked against.
struct Version {
    int major;
    int minor;
    int patch;
};

inline Version version() noexcept {
    Version v{0, 0, 0};
    graph_lite_version(&v.major, &v.minor, &v.patch);
    return v;
}

inline const char* version_string() noexcept {
    return graph_lite_version_string();
}

/// Thrown by the checked helpers below. Carries the code, so a caller that cares
/// about the distinction can still recover it instead of parsing a message.
class Error : public std::runtime_error {
public:
    explicit Error(graph_lite_status status)
        : std::runtime_error(graph_lite_status_string(status)), status_(status) {}

    graph_lite_status status() const noexcept { return status_; }

private:
    graph_lite_status status_;
};

/// Turn a status code into an exception, for call sites that cannot usefully
/// continue. Call sites that can should keep checking the code directly.
inline void check(graph_lite_status status) {
    if (status != GRAPH_LITE_OK)
        throw Error(status);
}

/// RAII owner of a graph handle. Move-only: exactly one owner, so the handle is
/// destroyed exactly once. Every method is the same-named C entry point through
/// check() — no state, no extra logic, and nothing exported beyond what
/// graph-lite.h already publishes.
class Graph {
public:
    /// Creates the underlying graph. Throws Error(GRAPH_LITE_ERR_OUT_OF_MEMORY)
    /// rather than smuggling a null handle into scope.
    Graph() { check(graph_lite_graph_create(&handle_)); }

    ~Graph() { graph_lite_graph_destroy(handle_); }

    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    Graph(Graph&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

    Graph& operator=(Graph&& other) noexcept {
        if (this != &other) {
            graph_lite_graph_destroy(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    graph_lite_id add_node(graph_lite_tag tag) {
        graph_lite_id id = 0;
        check(graph_lite_add_node(handle_, tag, &id));
        return id;
    }

    void remove_node(graph_lite_id id) { check(graph_lite_remove_node(handle_, id)); }

    graph_lite_tag node_tag(graph_lite_id id) {
        graph_lite_tag tag = 0;
        check(graph_lite_node_tag(handle_, id, &tag));
        return tag;
    }

    graph_lite_id add_edge(graph_lite_id from, graph_lite_id to, graph_lite_tag tag) {
        graph_lite_id id = 0;
        check(graph_lite_add_edge(handle_, from, to, tag, &id));
        return id;
    }

    void remove_edge(graph_lite_id id) { check(graph_lite_remove_edge(handle_, id)); }

    graph_lite_tag edge_tag(graph_lite_id id) {
        graph_lite_tag tag = 0;
        check(graph_lite_edge_info(handle_, id, nullptr, nullptr, &tag));
        return tag;
    }

    /// The edge ids of one node's forward (or reverse) adjacency, ascending.
    std::vector<graph_lite_id> out_edges(graph_lite_id id) const {
        return edge_ids_(graph_lite_out_edges, id);
    }

    std::vector<graph_lite_id> in_edges(graph_lite_id id) const {
        return edge_ids_(graph_lite_in_edges, id);
    }

    std::size_t node_count() const { return graph_lite_node_count(handle_); }

    std::size_t edge_count() const { return graph_lite_edge_count(handle_); }

    /// The int-valued property of an owner; doubles/bools/strings have their
    /// own spellings. Throws Error for a missing owner or key.
    int64_t get_int(graph_lite_kind kind, graph_lite_id id, const char* key) {
        return value_(int64_t(0), graph_lite_get_int, kind, id, key);
    }

    double get_double(graph_lite_kind kind, graph_lite_id id, const char* key) {
        return value_(0.0, graph_lite_get_double, kind, id, key);
    }

    bool get_bool(graph_lite_kind kind, graph_lite_id id, const char* key) {
        int value = 0;
        check(graph_lite_get_bool(handle_, kind, id, key, &value));
        return value != 0;
    }

    std::string get_string(graph_lite_kind kind, graph_lite_id id, const char* key) {
        return get_string_(kind, id, key);
    }

    void set_int(graph_lite_kind kind, graph_lite_id id, const char* key, int64_t value) {
        check(graph_lite_set_int(handle_, kind, id, key, value));
    }

    void set_double(graph_lite_kind kind, graph_lite_id id, const char* key, double value) {
        check(graph_lite_set_double(handle_, kind, id, key, value));
    }

    void set_bool(graph_lite_kind kind, graph_lite_id id, const char* key, bool value) {
        check(graph_lite_set_bool(handle_, kind, id, key, value));
    }

    void set_string(graph_lite_kind kind, graph_lite_id id, const char* key, const char* value) {
        check(graph_lite_set_string(handle_, kind, id, key, value));
    }

    void remove_property(graph_lite_kind kind, graph_lite_id id, const char* key) {
        check(graph_lite_remove_property(handle_, kind, id, key));
    }

    uint64_t memory_usage() const {
        uint64_t bytes = 0;
        check(graph_lite_graph_memory_usage(handle_, &bytes));
        return bytes;
    }

    uint64_t out_degree(graph_lite_id id) const { return degree_(graph_lite_node_out_degree, id); }

    uint64_t in_degree(graph_lite_id id) const { return degree_(graph_lite_node_in_degree, id); }

    /// Neighbor ids, in the unspecified order the C header specifies. Sized
    /// exactly: the degree query and the copy are the same two calls any
    /// binding makes.
    std::vector<graph_lite_id> out_neighbors(graph_lite_id id) const {
        return neighbors_(graph_lite_node_out_degree, graph_lite_node_out_neighbors, id);
    }

    std::vector<graph_lite_id> in_neighbors(graph_lite_id id) const {
        return neighbors_(graph_lite_node_in_degree, graph_lite_node_in_neighbors, id);
    }

    struct Walk {
        std::vector<graph_lite_id> ids;
        bool partial;
    };

    Walk traverse(graph_lite_id start, graph_lite_walk_order order,
                  graph_lite_walk_direction direction, uint64_t max_depth = UINT64_MAX,
                  uint64_t max_results = UINT64_MAX, bool filter_node_tag = false,
                  graph_lite_tag node_tag = 0, bool filter_edge_tag = false,
                  graph_lite_tag edge_tag = 0) const {
        graph_lite_traversal* traversal = nullptr;
        const graph_lite_status status = graph_lite_traversal_open(
            handle_, start, order, direction, filter_node_tag, node_tag, filter_edge_tag, edge_tag,
            max_depth, max_results, &traversal);
        check(status);

        struct TraversalDeleter {
            void operator()(graph_lite_traversal* value) const {
                graph_lite_traversal_destroy(value);
            }
        };

        std::unique_ptr<graph_lite_traversal, TraversalDeleter> owner(traversal);
        Walk result{{}, false};
        for (graph_lite_id id = 0; graph_lite_traversal_next(owner.get(), &id) == GRAPH_LITE_OK;)
            result.ids.push_back(id);
        int partial = 0;
        check(graph_lite_traversal_partial(owner.get(), &partial));
        result.partial = partial != 0;
        return result;
    }

    std::vector<graph_lite_id> any_path(graph_lite_id start, graph_lite_id target,
                                        graph_lite_walk_direction direction,
                                        bool filter_node_tag = false, graph_lite_tag node_tag = 0,
                                        bool filter_edge_tag = false,
                                        graph_lite_tag edge_tag = 0) const {
        return path_(graph_lite_any_path, start, target, direction, filter_node_tag, node_tag,
                     filter_edge_tag, edge_tag);
    }

    std::vector<graph_lite_id>
    shortest_path(graph_lite_id start, graph_lite_id target, graph_lite_walk_direction direction,
                  bool filter_node_tag = false, graph_lite_tag node_tag = 0,
                  bool filter_edge_tag = false, graph_lite_tag edge_tag = 0) const {
        return path_(graph_lite_shortest_path, start, target, direction, filter_node_tag, node_tag,
                     filter_edge_tag, edge_tag);
    }

private:
    using DegreeFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_id, uint64_t*);
    using NeighborFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_id, graph_lite_id*,
                                             uint64_t, uint64_t*);
    using EdgeListFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_id, graph_lite_id*,
                                             size_t, size_t*);
    using IntFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_kind, graph_lite_id,
                                        const char*, int64_t*);
    using DoubleFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_kind, graph_lite_id,
                                           const char*, double*);
    using BoolFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_kind, graph_lite_id,
                                         const char*, bool*);
    using PathFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_id, graph_lite_id,
                                         graph_lite_walk_direction, int, graph_lite_tag, int,
                                         graph_lite_tag, graph_lite_id*, uint64_t, uint64_t*);

    uint64_t degree_(DegreeFn degree, graph_lite_id id) const {
        uint64_t value = 0;
        check(degree(handle_, id, &value));
        return value;
    }

    std::vector<graph_lite_id> neighbors_(DegreeFn degree, NeighborFn neighbors,
                                          graph_lite_id id) const {
        uint64_t count = 0;
        check(degree(handle_, id, &count));
        std::vector<graph_lite_id> result(count);
        if (count > 0)
            check(neighbors(handle_, id, result.data(), count, &count));
        return result;
    }

    std::vector<graph_lite_id> edge_ids_(EdgeListFn edges, graph_lite_id id) const {
        size_t count = 0;
        check(edges(handle_, id, nullptr, 0, &count)); // the how-many probe
        std::vector<graph_lite_id> result(count);
        if (count > 0)
            check(edges(handle_, id, result.data(), result.size(), &count));
        return result;
    }

    std::vector<graph_lite_id> path_(PathFn path, graph_lite_id start, graph_lite_id target,
                                     graph_lite_walk_direction direction, bool filter_node_tag,
                                     graph_lite_tag node_tag, bool filter_edge_tag,
                                     graph_lite_tag edge_tag) const {
        uint64_t count = 0;
        const graph_lite_status first =
            path(handle_, start, target, direction, filter_node_tag, node_tag, filter_edge_tag,
                 edge_tag, nullptr, 0, &count);
        if (first != GRAPH_LITE_ERR_BUFFER_TOO_SMALL)
            check(first);
        std::vector<graph_lite_id> result(count);
        check(path(handle_, start, target, direction, filter_node_tag, node_tag, filter_edge_tag,
                   edge_tag, result.data(), result.size(), &count));
        return result;
    }

    template <typename T, typename GetFn>
    T value_(T fallback, GetFn get, graph_lite_kind kind, graph_lite_id id, const char* key) {
        T value = fallback;
        check(get(handle_, kind, id, key, &value));
        return value;
    }

    // Strings come back through a caller-owned buffer: the size query returns
    // the length excluding the NUL, so the copy needs len + 1 bytes of room.
    std::string get_string_(graph_lite_kind kind, graph_lite_id id, const char* key) {
        size_t len = 0;
        check(graph_lite_get_string(handle_, kind, id, key, nullptr, 0, &len));
        std::string result(len + 1, '\0');
        check(graph_lite_get_string(handle_, kind, id, key, result.data(), result.size(), &len));
        result.resize(len);
        return result;
    }

    graph_lite_graph* handle_ = nullptr;
};

} // namespace graph_lite

#endif // GRAPH_LITE_HPP
