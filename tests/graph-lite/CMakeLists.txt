# The C translation unit is compiled as C and linked into the C++ test binary:
# that is what proves the public header is usable from a non-C++ caller.
ulsp_add_test(graph_lite_version_test
    SOURCES version_test.cpp c_abi_test.c
    LINK    graph-lite::graph-lite)

# Deep store/walk semantics, expressed in the generic model only.
ulsp_add_test(graph_lite_model_test
    SOURCES model_test.cpp
    LINK    graph-lite::graph-lite)
