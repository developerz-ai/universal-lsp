# Tests mirror the repo's internal boundaries, on purpose:
#   tests/graph-lite/  generic graph correctness + persistence fuzz/crash-consistency.
#                      Zero LSP vocabulary — the forcing function for API cleanliness.
#   tests/engine/      protocol-level + scheduling tests.
#   tests/plugins/     per-language golden-file tests: feed a plugin a sample file,
#                      assert the scope tree / highlights / diagnostics match.

# One helper so every suite is registered with CTest the same way. A suite added
# by hand is a suite that eventually stops being run.
function(ulsp_add_test name)
    cmake_parse_arguments(T "" "" "SOURCES;LINK" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE ${T_LINK} GTest::gtest_main ulsp_warnings ulsp_sanitizers)
    gtest_discover_tests(${name}
        PROPERTIES LABELS "unit"
        DISCOVERY_TIMEOUT 30)
endfunction()

add_subdirectory(graph-lite)
add_subdirectory(engine)
add_subdirectory(plugins)
