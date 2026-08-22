// Nothing under tests/graph-lite/ may name a caller's domain concept. If a test
// here needs borrowed vocabulary to describe itself, the API leaked and the
// extraction just got more expensive. Guard: scripts/lint/graph-lite-vocabulary.ts
#include <gtest/gtest.h>

#include "graph-lite.h"

TEST(Version, LinkedLibraryMatchesHeader) {
    const auto v = graph_lite::version();
    EXPECT_EQ(v.major, GRAPH_LITE_VERSION_MAJOR);
    EXPECT_EQ(v.minor, GRAPH_LITE_VERSION_MINOR);
    EXPECT_EQ(v.patch, GRAPH_LITE_VERSION_PATCH);
}

TEST(Version, StringMatchesComponents) {
    EXPECT_STREQ(graph_lite::version_string(), GRAPH_LITE_VERSION_STRING);
}
