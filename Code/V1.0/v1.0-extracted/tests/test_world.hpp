#pragma once

// One in-memory school for the tests. The clock is a plain variable so a test
// can move time forward and hit the deadline boundaries exactly.

#include <memory>

#include "lms/database.hpp"
#include "lms/password.hpp"
#include "lms/services.hpp"
#include "lms/sqlite_repository.hpp"

namespace lms::testing {

struct World {
  World() {
    setHashIterations(1000);  // fast, the format is the same as production
    auto get = [this](int id) {
      auto rec = repo.findUser(id);
      return UserFactory::fromRecord(*rec, repo.departmentOfHod(id).value_or(0));
    };
    adminUser = UserFactory::create(Role::kAdmin, "root", "Root Admin", "Admin#1234");
    int adminId = repo.addUser(adminUser->record());
    adminUser = get(adminId);
    int hodId = admin.createUser(*adminUser, Role::kHod, "hod", "Head", "Hod#12345");
    int lecId = admin.createUser(*adminUser, Role::kLecturer, "lec", "Lecturer", "Lec#12345");
    int s1 = admin.createUser(*adminUser, Role::kStudent, "stu1", "Student One", "Stu#12345");
    int s2 = admin.createUser(*adminUser, Role::kStudent, "stu2", "Student Two", "Stu#12345");
    dept = admin.createDepartment(*adminUser, "Sciences");
    admin.assignHod(*adminUser, dept, hodId);
    course = admin.createCourse(*adminUser, dept, "CS101", "Computer Studies");
    admin.assignLecturer(*adminUser, course, lecId);
    admin.enrol(*adminUser, s1, course);
    admin.enrol(*adminUser, s2, course);
    hodUser = get(hodId);
    lecturer = get(lecId);
    student = get(s1);
    student2 = get(s2);
    deadline = now + 7 * kSecondsPerDay;
    assignment = assignments.createAssignment(*lecturer, course, "Lab report",
                                              "sorting", deadline, 20);
  }

  /// Student 1 submits on time, returns the submission id.
  int submitOnTime(const User& who) {
    return submissions.submit(who, assignment, "my work", false).submissionId;
  }
  /// Submits `seconds` after the deadline.
  int submitLate(const User& who, Time seconds) {
    now = deadline + seconds;
    return submissions.submit(who, assignment, "late work", true).submissionId;
  }

  Database db{":memory:"};
  SqliteRepository repo{db};
  Time now = parseTime("2026-11-10 12:00");
  Clock clock = [this] { return now; };
  StandardPenaltyStrategy penalty;

  AuthService auth{repo, clock};
  AdminService admin{repo, repo, repo, clock};
  AssignmentService assignments{repo, repo, repo, repo, repo, repo, clock};
  SubmissionService submissions{repo, repo, repo, repo, penalty, clock};
  MarkingService marking{repo, repo, repo, repo, repo, repo, penalty, clock};
  ModerationService moderation{repo, repo, repo, repo, repo, penalty, clock};

  std::unique_ptr<User> adminUser, hodUser, lecturer, student, student2;
  int dept = 0, course = 0, assignment = 0;
  Time deadline = 0;
};

}  // namespace lms::testing
