# The C translation unit is compiled as C and linked into the C++ test binary:
# that is what proves the public header is usable from a non-C++ caller.
ulsp_add_test(graph_lite_version_test
    SOURCES version_test.cpp c_abi_test.c
    LINK    graph-lite::graph-lite)

# Mutation-consistency suite: a shadow adjacency model is compared against the
# store after every operation of a fixed-seed churn.
ulsp_add_test(graph_lite_store_test
    SOURCES store_test.cpp
    LINK    graph-lite::graph-lite)

ulsp_add_test(graph_lite_slot_pool_test
    SOURCES slot_pool_test.cpp
    LINK    graph-lite::graph-lite)

# Allocation-failure suite: a refusal at any allocation point of a mutating
# entry point surfaces as a status code (OOM from create), never as an
# exception escaping the C boundary.
ulsp_add_test(graph_lite_alloc_test
    SOURCES alloc_failure_test.cpp
    LINK    graph-lite::graph-lite)

# The model and typed-property suites: same public header, store internals
# invisible.
ulsp_add_test(graph_lite_model_test
    SOURCES model_test.cpp
    LINK    graph-lite::graph-lite)

ulsp_add_test(graph_lite_model_properties_test
    SOURCES model_properties_test.cpp
    LINK    graph-lite::graph-lite)
