#include <fstream>

#include "lms/error.hpp"
#include "lms/password.hpp"
#include "lms/services.hpp"

namespace lms {

void requireRole(const User& actor, Role role) {
  if (actor.role() != role) {
    throw PermissionError("your role is not allowed to do this");
  }
}

// ---------------------------------------------------------------- login
std::unique_ptr<User> AuthService::login(const std::string& username,
                                         const std::string& password) {
  auto rec = users_.findUserByName(username);
  if (!rec) throw ValidationError("wrong username or password");
  if (!rec->active) throw RuleError("this account is not active");
  if (rec->failedLogins >= kMaxFailedLogins) {
    throw RuleError("this account is locked, ask the administrator to reset it");
  }
  int deptId = 0;
  auto user = UserFactory::fromRecord(*rec);
  if (!user->authenticate(password)) {
    rec->failedLogins += 1;
    users_.updateUser(*rec);
    if (rec->failedLogins >= kMaxFailedLogins) {
      throw RuleError("too many wrong passwords, the account is now locked");
    }
    throw ValidationError("wrong username or password");
  }
  if (rec->failedLogins != 0) {
    rec->failedLogins = 0;
    users_.updateUser(*rec);
  }
  if (rec->role == Role::kHod) {
    deptId = users_.departmentOfHod(rec->id).value_or(0);
  }
  return UserFactory::fromRecord(*rec, deptId);
}

void AuthService::changePassword(const User& user, const std::string& oldPassword,
                                 const std::string& newPassword) {
  if (!user.authenticate(oldPassword)) {
    throw ValidationError("the old password is wrong");
  }
  requireValidPassword(newPassword);
  auto rec = users_.findUser(user.id());
  if (!rec) throw NotFoundError("user not found");
  rec->passwordHash = hashPassword(newPassword);
  users_.updateUser(*rec);
}

// ---------------------------------------------------------------- admin
int AdminService::createUser(const User& admin, Role role,
                             const std::string& username,
                             const std::string& fullName,
                             const std::string& password) {
  requireRole(admin, Role::kAdmin);
  auto user = UserFactory::create(role, username, fullName, password);
  if (users_.findUserByName(username)) {
    throw ValidationError("that username is already taken");
  }
  return users_.addUser(user->record());
}

void AdminService::setActive(const User& admin, int userId, bool active) {
  requireRole(admin, Role::kAdmin);
  auto rec = users_.findUser(userId);
  if (!rec) throw NotFoundError("user not found");
  if (userId == admin.id() && !active) {
    throw RuleError("you cannot deactivate your own account");
  }
  rec->active = active;
  users_.updateUser(*rec);
}

void AdminService::resetPassword(const User& admin, int userId,
                                 const std::string& newPassword) {
  requireRole(admin, Role::kAdmin);
  requireValidPassword(newPassword);
  auto rec = users_.findUser(userId);
  if (!rec) throw NotFoundError("user not found");
  rec->passwordHash = hashPassword(newPassword);
  rec->failedLogins = 0;
  users_.updateUser(*rec);
}

void AdminService::anonymiseUser(const User& admin, int userId) {
  requireRole(admin, Role::kAdmin);
  auto rec = users_.findUser(userId);
  if (!rec) throw NotFoundError("user not found");
  if (userId == admin.id()) throw RuleError("you cannot anonymise yourself");
  rec->username = "deleted-" + std::to_string(userId);
  rec->fullName = "Former user";
  rec->passwordHash = hashPassword("x" + std::to_string(clock_()) + "-unusable");
  rec->active = false;
  users_.updateUser(*rec);
}

std::vector<UserRecord> AdminService::listUsers(const User& admin) {
  requireRole(admin, Role::kAdmin);
  return users_.listUsers();
}

int AdminService::createDepartment(const User& admin, const std::string& name) {
  requireRole(admin, Role::kAdmin);
  if (name.empty() || name.size() > 80) {
    throw ValidationError("department name must be 1 to 80 characters");
  }
  return courses_.addDepartment(name);
}

void AdminService::assignHod(const User& admin, int deptId, int userId) {
  requireRole(admin, Role::kAdmin);
  if (!courses_.departmentExists(deptId)) {
    throw NotFoundError("department not found");
  }
  auto rec = users_.findUser(userId);
  if (!rec) throw NotFoundError("user not found");
  if (rec->role != Role::kHod) {
    throw RuleError("that user is not a Head of Department");
  }
  courses_.setHod(deptId, userId);
}

int AdminService::createCourse(const User& admin, int deptId,
                               const std::string& code,
                               const std::string& name) {
  requireRole(admin, Role::kAdmin);
  if (code.empty() || code.size() > 20 || name.empty() || name.size() > 80) {
    throw ValidationError("course code or name has a bad length");
  }
  if (!courses_.departmentExists(deptId)) {
    throw NotFoundError("department not found");
  }
  return courses_.addCourse(deptId, code, name);
}

void AdminService::assignLecturer(const User& admin, int courseId,
                                  int lecturerId) {
  requireRole(admin, Role::kAdmin);
  if (!courses_.findCourse(courseId)) throw NotFoundError("course not found");
  auto rec = users_.findUser(lecturerId);
  if (!rec) throw NotFoundError("user not found");
  if (rec->role != Role::kLecturer) throw RuleError("that user is not a lecturer");
  courses_.addLecturer(courseId, lecturerId);
}

void AdminService::enrol(const User& admin, int studentId, int courseId) {
  requireRole(admin, Role::kAdmin);
  if (!courses_.findCourse(courseId)) throw NotFoundError("course not found");
  auto rec = users_.findUser(studentId);
  if (!rec) throw NotFoundError("user not found");
  if (rec->role != Role::kStudent) throw RuleError("that user is not a student");
  courses_.enrol(studentId, courseId, clock_());
}

std::vector<AuditEntry> AdminService::auditLog(const User& admin) {
  requireRole(admin, Role::kAdmin);
  return audit_.listEntries();
}

void AdminService::exportAudit(const User& admin, const std::string& path) {
  requireRole(admin, Role::kAdmin);
  std::ofstream out(path);
  if (!out) throw Error("cannot write to " + path);
  for (const auto& e : audit_.listEntries()) {
    out << e.id << " | " << formatTime(e.at) << " | user " << e.actorId << " | "
        << toString(e.action) << " | ";
    if (e.submissionId) out << "submission " << *e.submissionId;
    else if (e.extAssignmentId && e.extStudentId) {
      out << "extension " << *e.extAssignmentId << "/" << *e.extStudentId;
    } else {
      out << "no target";  // the schema does not allow this, but do not crash
    }
    out << " | " << e.oldValue << " -> " << e.newValue << " | " << e.reason
        << "\n";
  }
}

}  // namespace lms
