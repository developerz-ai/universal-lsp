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

} // namespace graph_lite

#endif // GRAPH_LITE_HPP
