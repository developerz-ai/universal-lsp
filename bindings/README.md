# graph-lite reference bindings

These files are executable ABI probes, not supported packages, wrappers, or a
language-specific distribution. Each runtime loads graph-lite's shared C ABI
directly; no C or C++ shim and no extension compilation is involved.

## Prerequisites

- CMake 3.24+ and Ninja to build graph-lite.
- Ruby plus the external [`ffi`](https://github.com/ffi/ffi) gem.
- Bun for the `bun:ffi` probe.
- Python 3 plus the external [`cffi`](https://cffi.readthedocs.io/) package. The
  Python probe uses cffi ABI mode (`cdef` + `dlopen`) only; it does not compile an
  extension.

Build the shared library from the repository root:

```bash
(cd graph-lite && cmake --preset default && cmake --build --preset default)
```

The default standalone preset writes the platform library to
`graph-lite/build/` (`libgraph-lite.so` on Linux, `libgraph-lite.dylib` on macOS,
or `graph-lite.dll` on Windows). Set its exact path when running a probe:

```bash
GRAPH_LITE_LIBRARY=graph-lite/build/libgraph-lite.so ruby bindings/ruby/reference.rb
GRAPH_LITE_LIBRARY=graph-lite/build/libgraph-lite.so bun bindings/javascript/reference.ts
GRAPH_LITE_LIBRARY=graph-lite/build/libgraph-lite.so python3 bindings/python/reference.py
```

Use the corresponding platform filename when it differs. `GRAPH_LITE_LIBRARY`
is required so the probes never guess a loader search path.

## Handle lifetime

Each probe destroys its graph, clears its own handle to `NULL`, then checks the
documented invalid-argument result through that null sentinel. A call using the
old non-null pointer after `graph_lite_graph_destroy` is undefined behavior: the
C ABI requires a graph to be released exactly once and does not assign a status
code to stale handles. This is actionable ABI friction for callers that need a
recoverable use-after-close result; do not treat it as a testable status-code
contract.
