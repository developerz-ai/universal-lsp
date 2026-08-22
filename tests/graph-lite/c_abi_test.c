/*
 * Compiled as C, not C++, on purpose.
 *
 * The public header claims to be usable from any language with an FFI, and the
 * cheapest honest test of that claim is a C translation unit that includes it and
 * links. A C++-only test would keep passing while a `bool`, a default argument,
 * or a stray `//`-in-a-macro made the header uncompilable for every binding —
 * and nothing would say so until an embedder tried.
 */
#include "graph-lite.h"

int graph_lite_c_abi_smoke(void);

int graph_lite_c_abi_smoke(void) {
    int major = -1, minor = -1, patch = -1;
    const char* text;

    graph_lite_version(&major, &minor, &patch);
    if (major != GRAPH_LITE_VERSION_MAJOR)
        return 1;
    if (minor != GRAPH_LITE_VERSION_MINOR)
        return 2;
    if (patch != GRAPH_LITE_VERSION_PATCH)
        return 3;

    /* Every out-parameter is optional; a binding that wants one component should
       not have to allocate three. */
    graph_lite_version(0, 0, 0);

    text = graph_lite_status_string(GRAPH_LITE_ERR_CORRUPT);
    if (text == 0)
        return 4;

    /* An out-of-range code must still return a string, never NULL. */
    text = graph_lite_status_string((graph_lite_status)9999);
    if (text == 0)
        return 5;

    return 0;
}
