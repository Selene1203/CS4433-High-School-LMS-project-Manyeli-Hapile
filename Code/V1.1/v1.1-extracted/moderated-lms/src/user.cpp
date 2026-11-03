#include "lms/user.hpp"

#include <algorithm>

#include "lms/error.hpp"
#include "lms/password.hpp"

namespace lms {

bool User::authenticate(const std::string& password) const {
  return verifyPassword(password, record_.passwordHash);
}

void requireValidPassword(const std::string& password) {
  if (password.size() < static_cast<std::size_t>(kMinPasswordLength)) {
    throw ValidationError("password must be at least 8 characters");
  }
}

namespace {

bool allowedUsernameChar(char c) {
  bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  bool digit = c >= '0' && c <= '9';
  return letter || digit || c == '.' || c == '_' || c == '-';
}

void requireValidUsername(const std::string& username) {
  if (username.empty() || username.size() > 40) {
    throw ValidationError("username must be 1 to 40 characters");
  }
  if (!std::all_of(username.begin(), username.end(), allowedUsernameChar)) {
    throw ValidationError("username has a character that is not allowed");
  }
}

}  // namespace

std::unique_ptr<User> UserFactory::create(Role role, const std::string& username,
                                          const std::string& fullName,
                                          const std::string& password) {
  requireValidUsername(username);
  if (fullName.empty() || fullName.size() > 80) {
    throw ValidationError("full name must be 1 to 80 characters");
  }
  requireValidPassword(password);
  UserRecord r;
  r.username = username;
  r.fullName = fullName;
  r.role = role;
  r.passwordHash = hashPassword(password);
  return fromRecord(r);
}

std::unique_ptr<User> UserFactory::fromRecord(const UserRecord& r, int deptId) {
  switch (r.role) {
    case Role::kAdmin: return std::make_unique<Administrator>(r);
    case Role::kLecturer: return std::make_unique<Lecturer>(r);
    case Role::kStudent: return std::make_unique<Student>(r);
    case Role::kHod: return std::make_unique<HeadOfDepartment>(r, deptId);
  }
  throw ValidationError("unknown role");
}

}  // namespace lms
