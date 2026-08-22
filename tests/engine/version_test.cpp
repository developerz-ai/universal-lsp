#include <gtest/gtest.h>

#include "engine/version.h"

TEST(EngineVersion, StringMatchesMacro) {
    EXPECT_STREQ(ulsp::version_string(), ULSP_VERSION_STRING);
}
