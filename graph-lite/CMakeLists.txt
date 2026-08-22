# graph-lite — embeddable property-graph storage + traversal.
#
# This file configures standalone as well as as a subdirectory. That is not a
# convenience: the repo-extraction decision (docs/spec.md phase 14) is only
# cheap if graph-lite never grows a dependency on the parent build, and the
# cheapest way to keep that true is for `cmake -S graph-lite` to work today.
#     cmake -S graph-lite -B build/graph-lite && cmake --build build/graph-lite
cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(graph-lite VERSION 0.0.1 DESCRIPTION "Embeddable property-graph library" LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    add_library(ulsp_warnings INTERFACE)
    add_library(ulsp_sanitizers INTERFACE)
endif()

# Versioned independently of the LSP engine, per docs/spec.md §2.
set(GRAPH_LITE_VERSION 0.0.1)

# The public surface is C (graph-lite.h); graph-lite.hpp is header-only
# ergonomics over it and exports nothing. Sources may be C++ internally — the ABI
# is what is constrained, not the implementation language.
add_library(graph-lite
    src/version.cpp)
add_library(graph-lite::graph-lite ALIAS graph-lite)

target_include_directories(graph-lite PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(graph-lite PUBLIC cxx_std_20)
target_link_libraries(graph-lite PRIVATE ulsp_warnings ulsp_sanitizers)
set_target_properties(graph-lite PROPERTIES VERSION ${GRAPH_LITE_VERSION})

# No `target_link_libraries(graph-lite PUBLIC <anything from this repo>)`, ever.
# Dependencies flow one way: engine -> graph-lite. Guard: scripts/lint/graph-lite-boundary.ts
