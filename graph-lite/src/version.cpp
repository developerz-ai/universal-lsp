#include "graph-lite.h"

namespace graph_lite {

Version version() noexcept {
    return Version{GRAPH_LITE_VERSION_MAJOR, GRAPH_LITE_VERSION_MINOR, GRAPH_LITE_VERSION_PATCH};
}

const char* version_string() noexcept {
    return GRAPH_LITE_VERSION_STRING;
}

}  // namespace graph_lite
