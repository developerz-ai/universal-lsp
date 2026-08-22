// graph-lite — a small, dependency-free, embeddable property-graph library.
//
// The single public header, in the spirit of SQLite's amalgamation: one include
// and a link line is the whole integration story.
//
// DATA MODEL (generic on purpose)
//   Typed nodes and typed, directed edges, both carrying arbitrary key/value
//   properties. Every domain concept a caller has is a node type or an edge type
//   that the CALLER defines and names; none of them appear here. Keeping this
//   header free of any caller's vocabulary is what makes the library reusable and
//   cheap to extract, and the rule is enforced mechanically rather than by review
//   discipline — see scripts/lint/graph-lite-vocabulary.ts.
//
// STATUS
//   Pre-implementation. This header exposes version information only. The
//   node/edge model, CSR-backed storage, traversal API and persistence format
//   land in build phase 1 — design in docs/graph-lite/README.md.
//
// Licensed under the MIT License. See graph-lite/LICENSE — deliberately more
// permissive than the engine that consumes it.
#ifndef GRAPH_LITE_H
#define GRAPH_LITE_H

#define GRAPH_LITE_VERSION_MAJOR 0
#define GRAPH_LITE_VERSION_MINOR 0
#define GRAPH_LITE_VERSION_PATCH 1
#define GRAPH_LITE_VERSION_STRING "0.0.1"

namespace graph_lite {

/// Compile-time version of the header the caller compiled against.
struct Version {
    int major;
    int minor;
    int patch;
};

/// Version of the library the caller actually linked against.
///
/// Compare against the GRAPH_LITE_VERSION_* macros to catch a header/binary
/// mismatch at startup rather than as a corrupt read much later.
Version version() noexcept;

/// Version string of the linked library, e.g. "0.0.1".
const char* version_string() noexcept;

} // namespace graph_lite

#endif // GRAPH_LITE_H
