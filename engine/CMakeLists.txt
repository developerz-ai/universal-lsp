# engine — the LSP daemon core: transport, sessions, workspaces, worker pools,
# scheduler, file-watch/debounce pipeline, Lua embedding, MCP front-end.
#
# Depends on graph-lite. graph-lite never depends back.
add_library(ulsp_engine
    src/version.cpp)
add_library(ulsp::engine ALIAS ulsp_engine)

target_include_directories(ulsp_engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(ulsp_engine
    PUBLIC  graph-lite::graph-lite
    PRIVATE ulsp_warnings ulsp_sanitizers)
