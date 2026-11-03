#include "lms/demo_data.hpp"

#include "lms/services.hpp"

namespace lms {

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
  auto admin = UserFactory::fromRecord(*repo.findUserByName("admin"));

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
  auto lecturer = UserFactory::fromRecord(*repo.findUser(lecId));
  AssignmentService asg(repo, repo, repo, repo, repo, repo, clock);
  int lab = asg.createAssignment(*lecturer, course, "Lab report",
                                 "Write up the sorting lab.",
                                 now - 5 * kSecondsPerDay, 20);
  asg.createAssignment(*lecturer, course, "Essay 1", "Two pages on networks.",
                       now + 7 * kSecondsPerDay, 30);

  SubmissionService subs(repo, repo, repo, repo, penalty, clock);
  auto st1 = UserFactory::fromRecord(*repo.findUser(s1));
  auto st2 = UserFactory::fromRecord(*repo.findUser(s2));
  clockNow = now - 6 * kSecondsPerDay;  // before the deadline
  subs.submit(*st1, lab, "Bubble sort and merge sort timings.", false);
  clockNow = now - 5 * kSecondsPerDay + 30 * 3600;  // 30 hours late, 2 days
  subs.submit(*st2, lab, "Late write-up of the sorting lab.", true);
}

}  // namespace lms
