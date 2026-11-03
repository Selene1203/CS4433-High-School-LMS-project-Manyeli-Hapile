#pragma once

#include <string>

namespace lms {

/// Salted PBKDF2-HMAC-SHA256 hash, stored as "pbkdf2$iterations$salt$hash".
/// OpenSSL does the work, nothing is hand rolled (NFR-03).
std::string hashPassword(const std::string& password);

/// True when the password matches the stored hash. Never throws for a
/// malformed hash, it just returns false.
bool verifyPassword(const std::string& password, const std::string& stored);

/// Tests lower this so they run fast. The default is 100000.
void setHashIterations(int iterations);

}  // namespace lms
