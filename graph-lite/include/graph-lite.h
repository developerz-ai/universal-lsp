/*
 * graph-lite — a small, dependency-free, embeddable property-graph library.
 *
 * THIS HEADER IS THE CONTRACT. It is C, with C linkage, and it is what every
 * embedder binds against: C, C++, Ruby (FFI), JavaScript (N-API / bun:ffi),
 * Python (ctypes / cffi), Rust, Go, and anything else with an FFI. C++ callers
 * who want ergonomics use graph-lite.hpp, which is header-only and implemented
 * entirely in terms of this file — it exports nothing of its own.
 *
 * That direction is deliberate and not negotiable per-function: C++ has no
 * stable ABI across compilers or standard-library versions, so a C++ signature
 * in a public header is unlinkable from another toolchain and unusable from any
 * non-C++ runtime. See docs/architecture/decisions/0004-graph-lite-c-abi.md.
 *
 * ABI RULES
 *   - No C++ type ever crosses this boundary.
 *   - Handles are opaque. Struct layout in a public header is an ABI promise.
 *   - Failures are integer codes, distinct per reason a caller must distinguish.
 *   - Buffers are caller-owned with an explicit size, or paired with an explicit
 *     free function — an embedder cannot free what a different allocator made.
 *   - Additive only. Changing an exported entry point breaks every binding
 *     silently, at load time.
 *
 * DATA MODEL (generic on purpose)
 *   Typed nodes and typed, directed edges, both carrying arbitrary key/value
 *   properties. Every domain concept a caller has is a node type or an edge type
 *   that the CALLER defines and names; none of them appear here. Keeping this
 *   header free of any caller's vocabulary is what makes the library reusable and
 *   cheap to extract, and the rule is enforced mechanically rather than by review
 *   discipline — see scripts/lint/graph-lite-vocabulary.ts.
 *
 * STATUS
 *   Pre-implementation. This header exposes version information only. The
 *   node/edge model, CSR-backed storage, traversal API and persistence format
 *   land in build phase 1 — design in docs/graph-lite/README.md.
 *
 * Licensed under the MIT License. See graph-lite/LICENSE — deliberately more
 * permissive than the engine that consumes it.
 */
#ifndef GRAPH_LITE_H
#define GRAPH_LITE_H

#define GRAPH_LITE_VERSION_MAJOR 0
#define GRAPH_LITE_VERSION_MINOR 0
#define GRAPH_LITE_VERSION_PATCH 1
#define GRAPH_LITE_VERSION_STRING "0.0.1"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Result codes. Every failure an embedder must tell apart gets its own code —
 * a single "error" collapses reasons that need different handling, and a
 * binding cannot recover a distinction the ABI never made.
 *
 * Additive only: existing values never change meaning.
 */
typedef enum graph_lite_status {
    GRAPH_LITE_OK = 0,
    GRAPH_LITE_ERR_INVALID_ARGUMENT = 1,
    GRAPH_LITE_ERR_OUT_OF_MEMORY = 2,
    GRAPH_LITE_ERR_NOT_FOUND = 3,
    GRAPH_LITE_ERR_IO = 4,
    GRAPH_LITE_ERR_CORRUPT = 5,
    GRAPH_LITE_ERR_VERSION_MISMATCH = 6,
    GRAPH_LITE_ERR_BUFFER_TOO_SMALL = 7,

    /* Forces the enum to int width, which makes the RANGE of the type cover every
     * int a caller could pass. Without it, a value outside the declared
     * enumerators has unspecified behaviour when converted — and a foreign
     * binding can and will pass an unknown code, because it was compiled against
     * a different version of this header. */
    GRAPH_LITE_STATUS_FORCE_INT = 0x7fffffff
} graph_lite_status;

/* Version of the library actually linked, as three out-parameters.
 * Compare against the GRAPH_LITE_VERSION_* macros to catch a header/binary
 * mismatch at startup rather than as a corrupt read much later. Any pointer may
 * be NULL if the caller does not want that component. */
void graph_lite_version(int* major, int* minor, int* patch);

/* Version string of the linked library, e.g. "0.0.1". Static storage; the caller
 * does not free it. */
const char* graph_lite_version_string(void);

/* Human-readable text for a status code. Static storage, never NULL — an unknown
 * code returns a string saying so rather than NULL, because a binding that has to
 * null-check an error-message getter will eventually forget. */
const char* graph_lite_status_string(graph_lite_status status);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GRAPH_LITE_H */
