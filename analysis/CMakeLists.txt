# analysis — the translation layer. Turns tree-sitter output and plugin Tier 2/3
# rules into calls against graph-lite's generic node/edge API: "symbol" becomes a
# node type, "calls" becomes an edge type.
#
# This is the ONLY place where LSP vocabulary and graph vocabulary are allowed to
# meet. Every concept name that graph-lite is forbidden to know lives here.
add_library(ulsp_analysis INTERFACE)
add_library(ulsp::analysis ALIAS ulsp_analysis)

target_include_directories(ulsp_analysis INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(ulsp_analysis INTERFACE graph-lite::graph-lite ulsp::engine)
