#include "engine/version.h"

#include <gtest/gtest.h>

TEST(EngineVersion, StringMatchesMacro) {
    EXPECT_STREQ(ulsp::version_string(), ULSP_VERSION_STRING);
}
