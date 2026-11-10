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

/// RAII owner of a store handle. Move-only: exactly one owner, so the handle is
/// destroyed exactly once. Every method is the same-named C entry point through
/// check() — no state, no extra logic, and nothing exported beyond what
/// graph-lite.h already publishes.
class Graph {
public:
    /// Creates the underlying store. Throws Error(GRAPH_LITE_ERR_OUT_OF_MEMORY)
    /// rather than smuggling a null handle into scope.
    Graph() {
        handle_ = graph_lite_graph_create();
        if (handle_ == nullptr)
            throw Error(GRAPH_LITE_ERR_OUT_OF_MEMORY);
    }

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

    graph_lite_id add_node(uint64_t type) {
        graph_lite_id id = 0;
        check(graph_lite_node_add(handle_, type, &id));
        return id;
    }

    void remove_node(graph_lite_id id) { check(graph_lite_node_remove(handle_, id)); }

    uint64_t node_type(graph_lite_id id) {
        uint64_t type = 0;
        check(graph_lite_node_type(handle_, id, &type));
        return type;
    }

    graph_lite_id add_edge(graph_lite_id from, graph_lite_id to, uint64_t type) {
        graph_lite_id id = 0;
        check(graph_lite_edge_add(handle_, from, to, type, &id));
        return id;
    }

    void remove_edge(graph_lite_id id) { check(graph_lite_edge_remove(handle_, id)); }

    uint64_t edge_type(graph_lite_id id) {
        uint64_t type = 0;
        check(graph_lite_edge_type(handle_, id, &type));
        return type;
    }

    uint64_t node_count() const {
        uint64_t count = 0;
        check(graph_lite_graph_node_count(handle_, &count));
        return count;
    }

    uint64_t edge_count() const {
        uint64_t count = 0;
        check(graph_lite_graph_edge_count(handle_, &count));
        return count;
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

private:
    using DegreeFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_id, uint64_t*);
    using NeighborFn = graph_lite_status (*)(const graph_lite_graph*, graph_lite_id, graph_lite_id*,
                                             uint64_t, uint64_t*);

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

    graph_lite_graph* handle_ = nullptr;
};

} // namespace graph_lite

#endif // GRAPH_LITE_HPP
