#pragma once

#include <memory>
#include <string>

#include "lms/types.hpp"

namespace lms {

/// One row of app_user.
struct UserRecord {
  int id = 0;
  std::string username;
  std::string passwordHash;
  std::string fullName;
  Role role = Role::kStudent;
  int failedLogins = 0;
  bool active = true;
};

/// Abstract user. The four roles only differ in role(), the permission checks
/// in the services use that (FR-02).
class User {
 public:
  explicit User(UserRecord record) : record_(std::move(record)) {}
  virtual ~User() = default;
  User(const User&) = default;
  User& operator=(const User&) = default;

  int id() const { return record_.id; }
  const std::string& username() const { return record_.username; }
  const std::string& fullName() const { return record_.fullName; }
  bool isActive() const { return record_.active; }
  bool isLocked() const { return record_.failedLogins >= kMaxFailedLogins; }
  const UserRecord& record() const { return record_; }

  /// True when the password matches the stored salted hash.
  bool authenticate(const std::string& password) const;

  virtual Role role() const = 0;

 private:
  UserRecord record_;
};

class Administrator final : public User {
 public:
  using User::User;
  Role role() const override { return Role::kAdmin; }
};

class Lecturer final : public User {
 public:
  using User::User;
  Role role() const override { return Role::kLecturer; }
};

class Student final : public User {
 public:
  using User::User;
  Role role() const override { return Role::kStudent; }
};

class HeadOfDepartment final : public User {
 public:
  HeadOfDepartment(UserRecord record, int deptId)
      : User(std::move(record)), deptId_(deptId) {}
  Role role() const override { return Role::kHod; }
  /// Department this person heads, 0 when none has been assigned yet.
  int deptId() const { return deptId_; }

 private:
  int deptId_;
};

/// The only place that decides which subclass to build and that hashes a new
/// password (Factory pattern).
class UserFactory {
 public:
  /// New, unsaved user (id 0). Checks the input and hashes the password.
  static std::unique_ptr<User> create(Role role, const std::string& username,
                                      const std::string& fullName,
                                      const std::string& password);
  /// Rebuilds the right subclass from a stored row.
  static std::unique_ptr<User> fromRecord(const UserRecord& record,
                                          int deptId = 0);
};

/// Shared input check used by the factory and the password change.
void requireValidPassword(const std::string& password);

}  // namespace lms
