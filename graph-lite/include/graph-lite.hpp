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

/// The NONE macros of the C header, spelled for C++ callers: same value, but
/// without the C-style cast the macros carry — which C++ builds that compile
/// with -Wold-style-cast (this repo's own default) reject at expansion.
inline constexpr graph_lite_node_id kNodeIdNone = 0;
inline constexpr graph_lite_edge_id kEdgeIdNone = 0;

/// Move-only owner of a store. Construction creates, destruction destroys —
/// no call site can leak a handle or double-free one. Methods throw Error
/// carrying the C call's status instead of returning codes by hand.
class Store {
public:
    /// Creates a new, empty store.
    Store() { check(graph_lite_store_create(&handle_)); }

    ~Store() { graph_lite_store_destroy(handle_); }

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    Store(Store&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Store& operator=(Store&& other) noexcept {
        if (this != &other) {
            graph_lite_store_destroy(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    /// The raw handle, for call sites that mix this header with the C ABI.
    graph_lite_store* handle() noexcept { return handle_; }

    const graph_lite_store* handle() const noexcept { return handle_; }

    graph_lite_node_id add_node(graph_lite_node_type type) {
        graph_lite_node_id id = kNodeIdNone;
        check(graph_lite_store_add_node(handle_, type, &id));
        return id;
    }

    graph_lite_edge_id add_edge(graph_lite_node_id from, graph_lite_node_id to,
                                graph_lite_edge_type type) {
        graph_lite_edge_id id = kEdgeIdNone;
        check(graph_lite_store_add_edge(handle_, from, to, type, &id));
        return id;
    }

    void remove_node(graph_lite_node_id id) { check(graph_lite_store_remove_node(handle_, id)); }

    void remove_edge(graph_lite_edge_id id) { check(graph_lite_store_remove_edge(handle_, id)); }

    uint64_t node_count() const {
        uint64_t count = 0;
        check(graph_lite_store_node_count(handle_, &count));
        return count;
    }

    uint64_t edge_count() const {
        uint64_t count = 0;
        check(graph_lite_store_edge_count(handle_, &count));
        return count;
    }

    graph_lite_node_type node_type(graph_lite_node_id id) const {
        graph_lite_node_type type = 0;
        check(graph_lite_store_node_type(handle_, id, &type));
        return type;
    }

    graph_lite_edge_type edge_type(graph_lite_edge_id id) const {
        graph_lite_edge_type type = 0;
        check(graph_lite_store_edge_type(handle_, id, &type));
        return type;
    }

    struct Endpoints {
        graph_lite_node_id from;
        graph_lite_node_id to;
    };

    Endpoints endpoints(graph_lite_edge_id id) const {
        Endpoints endpoints{kNodeIdNone, kNodeIdNone};
        check(graph_lite_store_edge_endpoints(handle_, id, &endpoints.from, &endpoints.to));
        return endpoints;
    }

private:
    graph_lite_store* handle_ = nullptr;
};

/// Move-only walk over a store, taken as a snapshot at construction. Reading
/// it never touches the store: the walk ran eagerly in the constructor, so
/// the store may change or be destroyed while the cursor is alive.
class Cursor {
public:
    /// Starts the walk. The filter pairs follow the C rule — pointer and
    /// count together, or neither; max_depth/max_results, 0 = unbounded.
    Cursor(const Store& store, graph_lite_node_id start, graph_lite_order order,
           graph_lite_direction direction, uint64_t max_depth = 0, uint64_t max_results = 0,
           const graph_lite_edge_type* edge_types = nullptr, uint64_t edge_type_count = 0,
           const graph_lite_node_type* node_types = nullptr, uint64_t node_type_count = 0) {
        check(graph_lite_cursor_open(store.handle(), start, order, direction, max_depth,
                                     max_results, edge_types, edge_type_count, node_types,
                                     node_type_count, &handle_));
    }

    ~Cursor() { graph_lite_cursor_free(handle_); }

    Cursor(const Cursor&) = delete;
    Cursor& operator=(const Cursor&) = delete;

    Cursor(Cursor&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Cursor& operator=(Cursor&& other) noexcept {
        if (this != &other) {
            graph_lite_cursor_free(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    /// Advances one node. False at the end of the walk — GRAPH_LITE_DONE,
    /// which is not an error and does not throw.
    bool next(graph_lite_node_id& out_node) {
        const graph_lite_status status = graph_lite_cursor_next(handle_, &out_node);
        if (status == GRAPH_LITE_DONE)
            return false;
        check(status);
        return true;
    }

    /// Depth of the node next() last returned; 0 before the first next().
    uint64_t depth() const {
        uint64_t depth = 0;
        check(graph_lite_cursor_depth(handle_, &depth));
        return depth;
    }

    struct Arrival {
        graph_lite_edge_id edge;
        graph_lite_node_id from;
    };

    /// How the walk reached the node next() last returned: the edge it
    /// crossed and the node it came from. For the start node: EDGE_ID_NONE
    /// and itself.
    Arrival arrived_via() const {
        Arrival arrival{kEdgeIdNone, kNodeIdNone};
        check(graph_lite_cursor_arrived_via(handle_, &arrival.edge, &arrival.from));
        return arrival;
    }

    /// True when a depth or result bound cut reachable nodes out of this walk.
    bool truncated() const {
        int flag = 0;
        check(graph_lite_cursor_truncated(handle_, &flag));
        return flag != 0;
    }

private:
    graph_lite_cursor* handle_ = nullptr;
};

/// Breadth-first shortest path over outgoing edges, as node ids from `from`
/// to `to`. Sizes its buffer with the required-size query, so a caller never
/// sees ERR_BUFFER_TOO_SMALL here: unknown ids throw NOT_FOUND, disconnected
/// endpoints throw UNREACHABLE.
inline std::vector<graph_lite_node_id>
shortest_path(const Store& store, graph_lite_node_id from, graph_lite_node_id to,
              const graph_lite_edge_type* edge_types = nullptr, uint64_t edge_type_count = 0) {
    uint64_t length = 0;
    uint64_t required = 0;
    check(graph_lite_shortest_path(store.handle(), from, to, edge_types, edge_type_count, nullptr,
                                   0, &length, &required));
    std::vector<graph_lite_node_id> path(required);
    check(graph_lite_shortest_path(store.handle(), from, to, edge_types, edge_type_count,
                                   path.data(), static_cast<uint64_t>(path.size()), &length,
                                   &required));
    return path;
}

} // namespace graph_lite

#endif // GRAPH_LITE_HPP
