import { dlopen, FFIType, ptr } from "bun:ffi";

const library = process.env.GRAPH_LITE_LIBRARY;
if (!library) throw new Error("set GRAPH_LITE_LIBRARY to the graph-lite shared library");

const OK = 0;
const INVALID_ARGUMENT = 1;
const BUFFER_TOO_SMALL = 7;
const END = 8;
const NODE = 0;
const CURSOR_EDGES = 1;

const { symbols } = dlopen(library, {
  graph_lite_status_string: { args: [FFIType.i32], returns: FFIType.cstring },
  graph_lite_graph_create: { args: [FFIType.ptr], returns: FFIType.i32 },
  graph_lite_graph_destroy: { args: [FFIType.ptr], returns: FFIType.void },
  graph_lite_add_node: { args: [FFIType.ptr, FFIType.u64, FFIType.ptr], returns: FFIType.i32 },
  graph_lite_add_edge: { args: [FFIType.ptr, FFIType.u64, FFIType.u64, FFIType.u64, FFIType.ptr], returns: FFIType.i32 },
  graph_lite_out_edges: { args: [FFIType.ptr, FFIType.u64, FFIType.ptr, FFIType.usize, FFIType.ptr], returns: FFIType.i32 },
  graph_lite_set_string: { args: [FFIType.ptr, FFIType.i32, FFIType.u64, FFIType.cstring, FFIType.cstring], returns: FFIType.i32 },
  graph_lite_get_string: { args: [FFIType.ptr, FFIType.i32, FFIType.u64, FFIType.cstring, FFIType.ptr, FFIType.usize, FFIType.ptr], returns: FFIType.i32 },
  graph_lite_cursor_open: { args: [FFIType.ptr, FFIType.i32, FFIType.u64, FFIType.ptr], returns: FFIType.i32 },
  graph_lite_cursor_next: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.i32 },
  graph_lite_cursor_destroy: { args: [FFIType.ptr], returns: FFIType.void },
});

function expect(actual: number, expected: number, operation: string): void {
  if (actual !== expected) throw new Error(`${operation}: expected ${expected}, got ${actual} (${symbols.graph_lite_status_string(actual)})`);
}

function pointerSlot(): BigUint64Array {
  return new BigUint64Array(1);
}

function idSlot(): BigUint64Array {
  return new BigUint64Array(1);
}

function sizeSlot(): BigUint64Array {
  return new BigUint64Array(1);
}

const graphSlot = pointerSlot();
expect(symbols.graph_lite_graph_create(ptr(graphSlot)), OK, "create");
let graph = graphSlot[0];

const firstSlot = idSlot();
const secondSlot = idSlot();
expect(symbols.graph_lite_add_node(graph, 10n, ptr(firstSlot)), OK, "add first node");
expect(symbols.graph_lite_add_node(graph, 20n, ptr(secondSlot)), OK, "add second node");
const first = firstSlot[0];
const second = secondSlot[0];

const edgeSlot = idSlot();
expect(symbols.graph_lite_add_edge(graph, first, second, 30n, ptr(edgeSlot)), OK, "add edge");
const edge = edgeSlot[0];

expect(symbols.graph_lite_set_string(graph, NODE, first, "label", "alpha"), OK, "set property");
const lengthSlot = sizeSlot();
expect(symbols.graph_lite_get_string(graph, NODE, first, "label", null, 0, ptr(lengthSlot)), BUFFER_TOO_SMALL, "property size query");
if (lengthSlot[0] !== 5n) throw new Error("property length was not reported");
const property = new Uint8Array(6);
expect(symbols.graph_lite_get_string(graph, NODE, first, "label", ptr(property), property.length, ptr(lengthSlot)), OK, "get property");
if (new TextDecoder().decode(property.subarray(0, 5)) !== "alpha") throw new Error("property value mismatch");

const edgeCount = sizeSlot();
expect(symbols.graph_lite_out_edges(graph, first, null, 0, ptr(edgeCount)), BUFFER_TOO_SMALL, "edge size query");
if (edgeCount[0] !== 1n) throw new Error("edge count was not reported");

const cursorSlot = pointerSlot();
expect(symbols.graph_lite_cursor_open(graph, CURSOR_EDGES, 0n, ptr(cursorSlot)), OK, "open cursor");
const cursor = cursorSlot[0];
const cursorId = idSlot();
expect(symbols.graph_lite_cursor_next(cursor, ptr(cursorId)), OK, "cursor next");
if (cursorId[0] !== edge) throw new Error("cursor yielded the wrong edge");
expect(symbols.graph_lite_cursor_next(cursor, ptr(cursorId)), END, "cursor exhaustion");
symbols.graph_lite_cursor_destroy(cursor);

if (symbols.graph_lite_status_string(12_345) !== "unknown status") throw new Error("unknown status was not readable");
symbols.graph_lite_graph_destroy(graph);
graph = 0n;
expect(symbols.graph_lite_add_node(graph, 40n, ptr(firstSlot)), INVALID_ARGUMENT, "null graph after close");
console.log(`Bun FFI ABI probe passed: ${library}`);
