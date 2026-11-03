#include <gtest/gtest.h>
#include <sqlite3.h>

#include <cstdio>
#include <string>

#include "lms/database.hpp"

namespace {

// Counts rows in sqlite_master for the audit index, using a plain connection.
int indexCount(const std::string& path) {
  sqlite3* raw = nullptr;
  EXPECT_EQ(sqlite3_open(path.c_str(), &raw), SQLITE_OK);
  sqlite3_stmt* st = nullptr;
  sqlite3_prepare_v2(raw,
                     "SELECT count(*) FROM sqlite_master WHERE "
                     "name='idx_audit_extension'",
                     -1, &st, nullptr);
  sqlite3_step(st);
  int n = sqlite3_column_int(st, 0);
  sqlite3_finalize(st);
  sqlite3_close(raw);
  return n;
}

void dropIndex(const std::string& path) {
  sqlite3* raw = nullptr;
  ASSERT_EQ(sqlite3_open(path.c_str(), &raw), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(raw, "DROP INDEX idx_audit_extension", nullptr, nullptr,
                         nullptr),
            SQLITE_OK);
  sqlite3_close(raw);
}

}  // namespace

// Replacing an extension makes SQLite check every audit entry that points at
// it, so the audit table needs an index on those two columns.
TEST(DatabaseIndex, NewDatabaseHasTheAuditExtensionIndex) {
  const std::string path = "index_test_new.db";
  std::remove(path.c_str());
  { lms::Database db(path); }
  EXPECT_EQ(indexCount(path), 1);
  std::remove(path.c_str());
}

TEST(DatabaseIndex, OldDatabaseGetsTheIndexWhenItIsOpened) {
  const std::string path = "index_test_old.db";
  std::remove(path.c_str());
  { lms::Database db(path); }
  dropIndex(path);  // makes it look like a v1.0 or v1.1 database
  ASSERT_EQ(indexCount(path), 0);
  { lms::Database db(path); }
  EXPECT_EQ(indexCount(path), 1);
  std::remove(path.c_str());
}
