# graph-lite standalone smoke tests — active only in standalone mode.
#
# Self-contained: builds against graph-lite's own sources/headers, links only
# graph-lite::graph-lite, uses no GTest, no parent ulsp_add_test helper, and no
# network fetch. graph-lite/CMakeLists.txt only add_subdirectory(tests) when
# graph-lite/ is the source root (CMAKE_SOURCE_DIR == CMAKE_CURRENT_SOURCE_DIR),
# so nothing here runs under add_subdirectory(graph-lite) from the parent build.

add_executable(standalone_smoke_test
    standalone_smoke_test.cpp)
target_link_libraries(standalone_smoke_test PRIVATE graph-lite::graph-lite)

add_test(NAME standalone_smoke_test COMMAND standalone_smoke_test)
