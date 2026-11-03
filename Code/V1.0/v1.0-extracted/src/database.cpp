#include "lms/database.hpp"

#include "lms/error.hpp"

namespace lms {
namespace {

std::string lastMessage(sqlite3* db) { return sqlite3_errmsg(db); }

}  // namespace

Database::Database(const std::string& path) {
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    std::string msg = db_ != nullptr ? lastMessage(db_) : "out of memory";
    sqlite3_close(db_);
    db_ = nullptr;
    throw DatabaseError("cannot open database: " + msg);
  }
  sqlite3_busy_timeout(db_, 2000);
  exec("PRAGMA foreign_keys = ON;");
  Statement check(*this,
                  "SELECT count(*) FROM sqlite_master WHERE name='app_user'");
  check.step();
  if (check.getInt(0) == 0) exec(schemaSql());
}

Database::~Database() {
  if (db_ != nullptr) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
  char* err = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err != nullptr ? err : "unknown error";
    sqlite3_free(err);
    throw DatabaseError(msg);
  }
}

std::int64_t Database::lastInsertId() const {
  return sqlite3_last_insert_rowid(db_);
}

void Database::backupTo(const std::string& path) {
  sqlite3* dest = nullptr;
  if (sqlite3_open(path.c_str(), &dest) != SQLITE_OK) {
    sqlite3_close(dest);
    throw DatabaseError("cannot create the backup file");
  }
  sqlite3_backup* b = sqlite3_backup_init(dest, "main", db_, "main");
  if (b == nullptr) {
    std::string msg = lastMessage(dest);
    sqlite3_close(dest);
    throw DatabaseError("backup failed: " + msg);
  }
  sqlite3_backup_step(b, -1);
  int rc = sqlite3_backup_finish(b);
  sqlite3_close(dest);
  if (rc != SQLITE_OK) throw DatabaseError("backup failed");
}

void Database::restoreFrom(const std::string& path) {
  sqlite3* src = nullptr;
  if (sqlite3_open_v2(path.c_str(), &src, SQLITE_OPEN_READONLY, nullptr) !=
      SQLITE_OK) {
    sqlite3_close(src);
    throw DatabaseError("cannot open the backup file");
  }
  sqlite3_backup* b = sqlite3_backup_init(db_, "main", src, "main");
  if (b == nullptr) {
    sqlite3_close(src);
    throw DatabaseError("restore failed");
  }
  sqlite3_backup_step(b, -1);
  int rc = sqlite3_backup_finish(b);
  sqlite3_close(src);
  if (rc != SQLITE_OK) throw DatabaseError("restore failed");
}

Statement::Statement(Database& db, const std::string& sql) : db_(db) {
  if (sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &stmt_, nullptr) !=
      SQLITE_OK) {
    throw DatabaseError(lastMessage(db.handle()));
  }
}

Statement::~Statement() { sqlite3_finalize(stmt_); }

Statement& Statement::bindInt(int i, std::int64_t v) {
  sqlite3_bind_int64(stmt_, i, v);
  return *this;
}
Statement& Statement::bindReal(int i, double v) {
  sqlite3_bind_double(stmt_, i, v);
  return *this;
}
Statement& Statement::bindText(int i, const std::string& v) {
  sqlite3_bind_text(stmt_, i, v.c_str(), static_cast<int>(v.size()),
                    SQLITE_TRANSIENT);
  return *this;
}
Statement& Statement::bindNull(int i) {
  sqlite3_bind_null(stmt_, i);
  return *this;
}

bool Statement::step() {
  int rc = sqlite3_step(stmt_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  throw DatabaseError(lastMessage(db_.handle()));
}

void Statement::run() { step(); }

std::int64_t Statement::getInt(int c) const {
  return sqlite3_column_int64(stmt_, c);
}
double Statement::getReal(int c) const { return sqlite3_column_double(stmt_, c); }
std::string Statement::getText(int c) const {
  const unsigned char* t = sqlite3_column_text(stmt_, c);
  return t != nullptr ? reinterpret_cast<const char*>(t) : "";
}
bool Statement::isNull(int c) const {
  return sqlite3_column_type(stmt_, c) == SQLITE_NULL;
}

}  // namespace lms
