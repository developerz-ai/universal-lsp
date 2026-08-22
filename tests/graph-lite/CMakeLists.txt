# The C translation unit is compiled as C and linked into the C++ test binary:
# that is what proves the public header is usable from a non-C++ caller.
ulsp_add_test(graph_lite_version_test
    SOURCES version_test.cpp c_abi_test.c
    LINK    graph-lite::graph-lite)
