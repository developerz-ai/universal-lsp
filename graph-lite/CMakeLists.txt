# graph-lite — embeddable property-graph storage + traversal.
#
# This file configures standalone as well as as a subdirectory. That is not a
# convenience: the repo-extraction decision (docs/spec.md phase 14) is only
# cheap if graph-lite never grows a dependency on the parent build, and the
# cheapest way to keep that true is for `cmake -S graph-lite` to work today.
#     cmake -S graph-lite -B build/graph-lite && cmake --build build/graph-lite
cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(graph-lite VERSION 0.1.0 DESCRIPTION "Embeddable property-graph library" LANGUAGES C CXX)
    include(CTest)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    add_library(ulsp_warnings INTERFACE)
    add_library(ulsp_sanitizers INTERFACE)
endif()

# Versioned independently of the LSP engine, per docs/spec.md §2.
set(GRAPH_LITE_VERSION 0.1.0)
set(GRAPH_LITE_VERSION_MAJOR 0)
set(GRAPH_LITE_VERSION_MINOR 1)
set(GRAPH_LITE_VERSION_PATCH 0)

# The public surface is C (graph-lite.h); graph-lite.hpp is header-only
# ergonomics over it and exports nothing. Sources may be C++ internally — the ABI
# is what is constrained, not the implementation language.
add_library(graph-lite SHARED
    src/version.cpp
    src/store.cpp
    src/graph.cpp
    src/model.cpp
    src/cursor.cpp
    src/traversal.cpp)
add_library(graph-lite::graph-lite ALIAS graph-lite)

target_include_directories(graph-lite PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(graph-lite PUBLIC cxx_std_20)
target_link_libraries(graph-lite PRIVATE ulsp_warnings ulsp_sanitizers)
target_compile_definitions(graph-lite PRIVATE
    GRAPH_LITE_BUILD
    GRAPH_LITE_BINARY_VERSION_MAJOR=${GRAPH_LITE_VERSION_MAJOR}
    GRAPH_LITE_BINARY_VERSION_MINOR=${GRAPH_LITE_VERSION_MINOR}
    GRAPH_LITE_BINARY_VERSION_PATCH=${GRAPH_LITE_VERSION_PATCH}
    GRAPH_LITE_BINARY_VERSION_STRING="${GRAPH_LITE_VERSION}")
set_target_properties(graph-lite PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION ${GRAPH_LITE_VERSION})

# The numbers behind docs/graph-lite/benchmark.md come from here. EXCLUDE_FROM_ALL:
# measuring is an explicit act (`cmake --build --preset release --target
# graph_lite_bench`), not something every build should pay for.
add_executable(graph_lite_bench EXCLUDE_FROM_ALL bench/store_bench.cpp)
target_link_libraries(graph_lite_bench PRIVATE graph-lite ulsp_warnings ulsp_sanitizers)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # This C executable is the extraction smoke test: the public header must
    # compile as C and a foreign caller must be able to use the ABI lifecycle.
    add_executable(graph_lite_embedding_smoke tests/embedding_smoke_test.c)
    target_link_libraries(graph_lite_embedding_smoke PRIVATE graph-lite)
    add_test(NAME graph_lite_embedding_smoke COMMAND graph_lite_embedding_smoke)
endif()

# No `target_link_libraries(graph-lite PUBLIC <anything from this repo>)`, ever.
# Dependencies flow one way: engine -> graph-lite. Guard: scripts/lint/graph-lite-boundary.ts
