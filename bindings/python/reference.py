import os

from cffi import FFI

library_path = os.environ["GRAPH_LITE_LIBRARY"]

ffi = FFI()
ffi.cdef(
    """
    typedef uint64_t graph_lite_id;
    typedef uint64_t graph_lite_tag;
    typedef struct graph_lite_graph graph_lite_graph;
    typedef struct graph_lite_cursor graph_lite_cursor;
    const char *graph_lite_status_string(int status);
    int graph_lite_graph_create(graph_lite_graph **out);
    void graph_lite_graph_destroy(graph_lite_graph *graph);
    int graph_lite_add_node(graph_lite_graph *graph, graph_lite_tag tag, graph_lite_id *out_id);
    int graph_lite_add_edge(graph_lite_graph *graph, graph_lite_id from, graph_lite_id to, graph_lite_tag tag, graph_lite_id *out_id);
    int graph_lite_out_edges(const graph_lite_graph *graph, graph_lite_id node, graph_lite_id *buffer, size_t capacity, size_t *count);
    int graph_lite_set_string(graph_lite_graph *graph, int kind, graph_lite_id id, const char *key, const char *value);
    int graph_lite_get_string(const graph_lite_graph *graph, int kind, graph_lite_id id, const char *key, char *buffer, size_t capacity, size_t *length);
    int graph_lite_cursor_open(const graph_lite_graph *graph, int kind, graph_lite_id node, graph_lite_cursor **out);
    int graph_lite_cursor_next(graph_lite_cursor *cursor, graph_lite_id *out_id);
    void graph_lite_cursor_destroy(graph_lite_cursor *cursor);
    """
)
lib = ffi.dlopen(library_path)

OK = 0
INVALID_ARGUMENT = 1
BUFFER_TOO_SMALL = 7
END = 8
NODE = 0
CURSOR_EDGES = 1


def expect(actual, expected, operation):
    if actual != expected:
        message = ffi.string(lib.graph_lite_status_string(actual)).decode()
        raise RuntimeError(f"{operation}: expected {expected}, got {actual} ({message})")


graph_out = ffi.new("graph_lite_graph **")
expect(lib.graph_lite_graph_create(graph_out), OK, "create")
graph = graph_out[0]

first_out = ffi.new("graph_lite_id *")
second_out = ffi.new("graph_lite_id *")
expect(lib.graph_lite_add_node(graph, 10, first_out), OK, "add first node")
expect(lib.graph_lite_add_node(graph, 20, second_out), OK, "add second node")
first = first_out[0]
second = second_out[0]

edge_out = ffi.new("graph_lite_id *")
expect(lib.graph_lite_add_edge(graph, first, second, 30, edge_out), OK, "add edge")
edge = edge_out[0]

expect(lib.graph_lite_set_string(graph, NODE, first, b"label", b"alpha"), OK, "set property")
length_out = ffi.new("size_t *")
expect(lib.graph_lite_get_string(graph, NODE, first, b"label", ffi.NULL, 0, length_out), BUFFER_TOO_SMALL, "property size query")
if length_out[0] != 5:
    raise RuntimeError("property length was not reported")
property_buffer = ffi.new("char[]", 6)
expect(lib.graph_lite_get_string(graph, NODE, first, b"label", property_buffer, 6, length_out), OK, "get property")
if ffi.string(property_buffer) != b"alpha":
    raise RuntimeError("property value mismatch")

edge_count = ffi.new("size_t *")
expect(lib.graph_lite_out_edges(graph, first, ffi.NULL, 0, edge_count), BUFFER_TOO_SMALL, "edge size query")
if edge_count[0] != 1:
    raise RuntimeError("edge count was not reported")

cursor_out = ffi.new("graph_lite_cursor **")
expect(lib.graph_lite_cursor_open(graph, CURSOR_EDGES, 0, cursor_out), OK, "open cursor")
cursor = cursor_out[0]
cursor_id = ffi.new("graph_lite_id *")
expect(lib.graph_lite_cursor_next(cursor, cursor_id), OK, "cursor next")
if cursor_id[0] != edge:
    raise RuntimeError("cursor yielded the wrong edge")
expect(lib.graph_lite_cursor_next(cursor, cursor_id), END, "cursor exhaustion")
lib.graph_lite_cursor_destroy(cursor)

if ffi.string(lib.graph_lite_status_string(12_345)) != b"unknown status":
    raise RuntimeError("unknown status was not readable")
lib.graph_lite_graph_destroy(graph)
graph = ffi.NULL
expect(lib.graph_lite_add_node(graph, 40, first_out), INVALID_ARGUMENT, "null graph after close")
print(f"Python cffi ABI probe passed: {library_path}")
