// Nothing under tests/graph-lite/ may name a caller's domain concept. If a test
// here needs borrowed vocabulary to describe itself, the API leaked and the
// extraction just got more expensive. Guard: scripts/lint/graph-lite-vocabulary.ts
#include "graph-lite.h"

#include "graph-lite.hpp"

#include <gtest/gtest.h>

extern "C" int graph_lite_c_abi_smoke(void);

TEST(Version, LinkedLibraryMatchesHeader) {
    const auto v = graph_lite::version();
    EXPECT_EQ(v.major, GRAPH_LITE_VERSION_MAJOR);
    EXPECT_EQ(v.minor, GRAPH_LITE_VERSION_MINOR);
    EXPECT_EQ(v.patch, GRAPH_LITE_VERSION_PATCH);
}

TEST(Version, StringMatchesComponents) {
    EXPECT_STREQ(graph_lite::version_string(), GRAPH_LITE_VERSION_STRING);
}

TEST(CAbi, HeaderCompilesAndLinksAsC) {
    // The assertions live in the .c file; a non-zero return names which one
    // failed. This test exists so the C translation unit is actually linked —
    // a file that compiles but is never linked proves nothing about the ABI.
    EXPECT_EQ(graph_lite_c_abi_smoke(), 0);
}

TEST(CAbi, StatusStringIsNeverNull) {
    EXPECT_STREQ(graph_lite_status_string(GRAPH_LITE_OK), "ok");
    EXPECT_NE(graph_lite_status_string(static_cast<graph_lite_status>(9999)), nullptr);
}

TEST(CAbi, ErrorCarriesTheCodeNotJustAMessage) {
    // A binding that only gets a message cannot branch on the reason.
    try {
        graph_lite::check(GRAPH_LITE_ERR_NOT_FOUND);
        FAIL() << "check() must throw on a non-OK status";
    } catch (const graph_lite::Error& e) {
        EXPECT_EQ(e.status(), GRAPH_LITE_ERR_NOT_FOUND);
    }
}
