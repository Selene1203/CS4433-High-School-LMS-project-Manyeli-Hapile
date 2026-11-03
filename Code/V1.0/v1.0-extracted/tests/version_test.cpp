#include <gtest/gtest.h>

#include <algorithm>

#include "lms/version.hpp"

// Smoke tests: prove the toolchain, SQLite linkage and test runner all work.
TEST(VersionTest, AppVersionIsSemanticVersion) {
  const std::string v = lms::appVersion();
  EXPECT_EQ(std::count(v.begin(), v.end(), '.'), 2);
}

TEST(VersionTest, SqliteIsLinked) { EXPECT_FALSE(lms::sqliteVersion().empty()); }
