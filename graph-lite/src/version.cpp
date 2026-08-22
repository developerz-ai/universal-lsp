#include "graph-lite.h"

extern "C" {

void graph_lite_version(int* major, int* minor, int* patch) {
    if (major)
        *major = GRAPH_LITE_VERSION_MAJOR;
    if (minor)
        *minor = GRAPH_LITE_VERSION_MINOR;
    if (patch)
        *patch = GRAPH_LITE_VERSION_PATCH;
}

const char* graph_lite_version_string(void) {
    return GRAPH_LITE_VERSION_STRING;
}

const char* graph_lite_status_string(graph_lite_status status) {
    /* A range-checked table, not a switch: the enum is int-wide by construction,
     * so a caller compiled against a DIFFERENT version of this header can pass a
     * code this build has never heard of. That must produce an ordinary string,
     * not an unhandled case nobody tested.
     *
     * Index == code. Adding a status means appending here and to the enum, in
     * that order, and never renumbering — the numbers are the ABI. */
    static const char* const kMessages[] = {
        "ok",        "invalid argument", "out of memory",    "not found",
        "i/o error", "corrupt data",     "version mismatch", "buffer too small",
    };
    const int code = static_cast<int>(status);
    const int count = static_cast<int>(sizeof(kMessages) / sizeof(kMessages[0]));

    if (code < 0 || code >= count) {
        /* Never NULL: a binding that has to null-check an error-message getter
         * will eventually forget, and then a bad code becomes a crash in someone
         * else's runtime instead of a readable string. */
        return "unknown status";
    }
    return kMessages[code];
}

} // extern "C"
