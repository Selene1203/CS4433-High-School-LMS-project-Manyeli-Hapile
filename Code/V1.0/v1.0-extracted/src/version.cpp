#include "lms/version.hpp"

#include <sqlite3.h>

namespace lms {

std::string appVersion() { return "1.1.0"; }

std::string sqliteVersion() { return sqlite3_libversion(); }

}  // namespace lms
