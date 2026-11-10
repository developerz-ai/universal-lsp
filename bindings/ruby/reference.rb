#!/usr/bin/env ruby
# frozen_string_literal: true

require "ffi"

library = ENV.fetch("GRAPH_LITE_LIBRARY")

module GraphLite
  extend FFI::Library

  ffi_lib ENV.fetch("GRAPH_LITE_LIBRARY")

  OK = 0
  INVALID_ARGUMENT = 1
  BUFFER_TOO_SMALL = 7
  CURSOR_END = 8
  NODE = 0
  CURSOR_EDGES = 1

  attach_function :graph_lite_status_string, [:int], :string
  attach_function :graph_lite_graph_create, [:pointer], :int
  attach_function :graph_lite_graph_destroy, [:pointer], :void
  attach_function :graph_lite_add_node, [:pointer, :uint64, :pointer], :int
  attach_function :graph_lite_add_edge, [:pointer, :uint64, :uint64, :uint64, :pointer], :int
  attach_function :graph_lite_out_edges, [:pointer, :uint64, :pointer, :size_t, :pointer], :int
  attach_function :graph_lite_set_string, [:pointer, :int, :uint64, :string, :string], :int
  attach_function :graph_lite_get_string, [:pointer, :int, :uint64, :string, :pointer, :size_t, :pointer], :int
  attach_function :graph_lite_cursor_open, [:pointer, :int, :uint64, :pointer], :int
  attach_function :graph_lite_cursor_next, [:pointer, :pointer], :int
  attach_function :graph_lite_cursor_destroy, [:pointer], :void
end

def expect(actual, expected, operation)
  return if actual == expected

  raise "#{operation}: expected #{expected}, got #{actual} (#{GraphLite.graph_lite_status_string(actual)})"
end

def id_value(pointer)
  pointer.read_uint64
end

def size_value(pointer)
  pointer.read_ulong
end

graph_out = FFI::MemoryPointer.new(:pointer)
expect(GraphLite.graph_lite_graph_create(graph_out), GraphLite::OK, "create")
graph = graph_out.read_pointer

first_out = FFI::MemoryPointer.new(:uint64)
second_out = FFI::MemoryPointer.new(:uint64)
expect(GraphLite.graph_lite_add_node(graph, 10, first_out), GraphLite::OK, "add first node")
expect(GraphLite.graph_lite_add_node(graph, 20, second_out), GraphLite::OK, "add second node")
first = id_value(first_out)
second = id_value(second_out)

edge_out = FFI::MemoryPointer.new(:uint64)
expect(GraphLite.graph_lite_add_edge(graph, first, second, 30, edge_out), GraphLite::OK, "add edge")
edge = id_value(edge_out)

expect(GraphLite.graph_lite_set_string(graph, GraphLite::NODE, first, "label", "alpha"), GraphLite::OK, "set property")
length_out = FFI::MemoryPointer.new(:size_t)
expect(GraphLite.graph_lite_get_string(graph, GraphLite::NODE, first, "label", nil, 0, length_out), GraphLite::BUFFER_TOO_SMALL, "property size query")
raise "property length was not reported" unless size_value(length_out) == 5
property = FFI::MemoryPointer.new(:char, 6)
expect(GraphLite.graph_lite_get_string(graph, GraphLite::NODE, first, "label", property, 6, length_out), GraphLite::OK, "get property")
raise "property value mismatch" unless property.read_string == "alpha"

edge_count = FFI::MemoryPointer.new(:size_t)
expect(GraphLite.graph_lite_out_edges(graph, first, nil, 0, edge_count), GraphLite::BUFFER_TOO_SMALL, "edge size query")
raise "edge count was not reported" unless size_value(edge_count) == 1

cursor_out = FFI::MemoryPointer.new(:pointer)
expect(GraphLite.graph_lite_cursor_open(graph, GraphLite::CURSOR_EDGES, 0, cursor_out), GraphLite::OK, "open cursor")
cursor = cursor_out.read_pointer
cursor_id = FFI::MemoryPointer.new(:uint64)
expect(GraphLite.graph_lite_cursor_next(cursor, cursor_id), GraphLite::OK, "cursor next")
raise "cursor yielded the wrong edge" unless id_value(cursor_id) == edge
expect(GraphLite.graph_lite_cursor_next(cursor, cursor_id), GraphLite::CURSOR_END, "cursor exhaustion")
GraphLite.graph_lite_cursor_destroy(cursor)

raise "unknown status was not readable" unless GraphLite.graph_lite_status_string(12_345) == "unknown status"
GraphLite.graph_lite_graph_destroy(graph)
graph = nil
expect(GraphLite.graph_lite_add_node(graph, 40, first_out), GraphLite::INVALID_ARGUMENT, "null graph after close")
puts "Ruby FFI ABI probe passed: #{library}"
