#include "lms/demo_data.hpp"

#include "lms/error.hpp"
#include "lms/services.hpp"

namespace lms {

namespace {

// The demo loader has just created these users, so a missing one means the
// database is broken. Say so instead of reading an empty optional.
UserRecord mustHave(const std::optional<UserRecord>& r, const char* what) {
  if (!r) throw Error(std::string("demo data: could not find ") + what);
  return *r;
}

}  // namespace

void createFirstAdmin(SqliteRepository& repo, const std::string& username,
                      const std::string& password) {
  auto user = UserFactory::create(Role::kAdmin, username, "Administrator",
                                  password);
  repo.addUser(user->record());
}

void loadDemoData(SqliteRepository& repo) {
  Time now = systemNow();
  Time clockNow = now - 10 * kSecondsPerDay;  // set back to build the history
  Clock clock = [&clockNow] { return clockNow; };

  createFirstAdmin(repo, "admin", "Admin#2026");
  auto admin = UserFactory::fromRecord(mustHave(repo.findUserByName("admin"), "admin"));

  AdminService adminSvc(repo, repo, repo, clock);
  int hodId = adminSvc.createUser(*admin, Role::kHod, "hod1", "Mr Mokoena",
                                  "Hod#2026pw");
  int lecId = adminSvc.createUser(*admin, Role::kLecturer, "lect1",
                                  "Ms Letsie", "Lect#2026pw");
  int s1 = adminSvc.createUser(*admin, Role::kStudent, "stu1", "Palesa Ntho",
                               "Stud#2026pw");
  int s2 = adminSvc.createUser(*admin, Role::kStudent, "stu2", "Thabo Mohale",
                               "Stud#2026pw");
  int dept = adminSvc.createDepartment(*admin, "Sciences");
  adminSvc.assignHod(*admin, dept, hodId);
  int course = adminSvc.createCourse(*admin, dept, "CS101", "Computer Studies");
  adminSvc.assignLecturer(*admin, course, lecId);
  adminSvc.enrol(*admin, s1, course);
  adminSvc.enrol(*admin, s2, course);

  StandardPenaltyStrategy penalty;
  auto lecturer = UserFactory::fromRecord(mustHave(repo.findUser(lecId), "lecturer"));
  AssignmentService asg(repo, repo, repo, repo, repo, repo, clock);
  int lab = asg.createAssignment(*lecturer, course, "Lab report",
                                 "Write up the sorting lab.",
                                 now - 5 * kSecondsPerDay, 20);
  asg.createAssignment(*lecturer, course, "Essay 1", "Two pages on networks.",
                       now + 7 * kSecondsPerDay, 30);

  SubmissionService subs(repo, repo, repo, repo, penalty, clock);
  auto st1 = UserFactory::fromRecord(mustHave(repo.findUser(s1), "student 1"));
  auto st2 = UserFactory::fromRecord(mustHave(repo.findUser(s2), "student 2"));
  clockNow = now - 6 * kSecondsPerDay;  // before the deadline
  subs.submit(*st1, lab, "Bubble sort and merge sort timings.", false);
  clockNow = now - 5 * kSecondsPerDay + 30L * 3600;  // 30 hours late, 2 days
  subs.submit(*st2, lab, "Late write-up of the sorting lab.", true);
}

}  // namespace lms
