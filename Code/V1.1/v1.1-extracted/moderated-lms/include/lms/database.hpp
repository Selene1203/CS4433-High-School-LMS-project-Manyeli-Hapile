#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <string>

namespace lms {

/// Owns the SQLite connection (RAII). Foreign keys are switched on for every
/// connection, and the schema is created when the file is new.
class Database {
 public:
  explicit Database(const std::string& path);
  ~Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  sqlite3* handle() const { return db_; }
  void exec(const std::string& sql);
  std::int64_t lastInsertId() const;
  /// Copies the whole database to another file (FR-22).
  void backupTo(const std::string& path);
  /// Replaces the contents of this database with a backup file (FR-22).
  void restoreFrom(const std::string& path);

 private:
  sqlite3* db_ = nullptr;
};

/// One prepared statement. Values are always bound, never concatenated into
/// the SQL text (NFR-04).
class Statement {
 public:
  Statement(Database& db, const std::string& sql);
  ~Statement();
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  Statement& bindInt(int index, std::int64_t value);
  Statement& bindReal(int index, double value);
  Statement& bindText(int index, const std::string& value);
  Statement& bindNull(int index);

  /// True when a row is ready, false when the statement is finished.
  bool step();
  /// Runs a statement that returns no rows.
  void run();

  std::int64_t getInt(int col) const;
  double getReal(int col) const;
  std::string getText(int col) const;
  bool isNull(int col) const;

 private:
  Database& db_;
  sqlite3_stmt* stmt_ = nullptr;
};

/// The SQL that creates every table, built from docs/phase3/db/schema.sql.
const char* schemaSql();

}  // namespace lms
