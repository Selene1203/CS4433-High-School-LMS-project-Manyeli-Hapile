#pragma once

#include <string>

namespace lms {

/// Application version (semantic versioning), e.g. "0.1.0".
std::string appVersion();

/// Version string of the SQLite library this binary is linked against.
std::string sqliteVersion();

}  // namespace lms
