#include <gtest/gtest.h>

#include "lms/error.hpp"
#include "test_world.hpp"

using namespace lms;
using lms::testing::World;

TEST(AuditLog, EntriesCannotBeChangedOrDeleted) {  // NFR-06, BR5
  World w;
  w.marking.mark(*w.lecturer, w.submitOnTime(*w.student), 10, "ok");
  EXPECT_THROW(w.db.exec("UPDATE audit_log SET reason='edited'"), DatabaseError);
  EXPECT_THROW(w.db.exec("DELETE FROM audit_log"), DatabaseError);
  EXPECT_EQ(w.repo.listEntries().size(), 1u);
}

TEST(AuditLog, FailedAuditRollsBackTheMark) {  // NFR-06, same transaction
  World w;
  int sub = w.submitOnTime(*w.student);
  // Make the audit insert fail by dropping the table's reason, then check
  // that the mark was not left behind.
  w.db.exec("DROP TRIGGER audit_no_update");
  w.db.exec("DROP TRIGGER audit_no_delete");
  w.db.exec("ALTER TABLE audit_log RENAME TO audit_old");
  EXPECT_THROW(w.marking.mark(*w.lecturer, sub, 10, "ok"), DatabaseError);
  EXPECT_FALSE(w.repo.findMark(sub).has_value());
}

TEST(Transaction, RollsBackWhenCommitIsNotReached) {
  World w;
  {
    Transaction tx(w.repo);
    w.repo.addDepartment("Temp");
  }
  EXPECT_THROW(w.admin.assignHod(*w.adminUser, 99, w.hodUser->id()), NotFoundError);
  Statement st(w.db, "SELECT count(*) FROM department WHERE name='Temp'");
  st.step();
  EXPECT_EQ(st.getInt(0), 0);
}

TEST(Injection, HostileTextIsStoredAsTextAndChangesNothing) {  // NFR-04
  World w;
  const char* hostile[] = {"'; DROP TABLE mark;--", "\" OR 1=1 --",
                           "x'); DELETE FROM app_user;--", "1; UPDATE mark SET raw_mark=20",
                           "' UNION SELECT password_hash FROM app_user --",
                           "%", "_", "\\", "' OR ''='", "Robert'); DROP TABLE app_user;--"};
  int sub = w.submitOnTime(*w.student);
  for (const char* h : hostile) {
    EXPECT_THROW(w.auth.login(h, h), Error);
    EXPECT_NO_THROW(w.marking.mark(*w.lecturer, sub, 10, h));
    EXPECT_NO_THROW(w.assignments.createAssignment(*w.lecturer, w.course, h, h,
                                                   w.deadline, 10));
  }
  EXPECT_EQ(w.repo.listUsers().size(), 5u);
  EXPECT_TRUE(w.repo.findMark(sub).has_value());
  EXPECT_DOUBLE_EQ(w.repo.findMark(sub)->rawMark, 10);
  EXPECT_EQ(w.repo.findMark(sub)->feedback, "Robert'); DROP TABLE app_user;--");
}

TEST(Constraints, DatabaseRefusesWhatTheCodeShouldNeverSend) {
  World w;
  int sub = w.submitOnTime(*w.student);
  // final mark above raw mark
  EXPECT_THROW(w.db.exec("INSERT INTO mark VALUES (" + std::to_string(sub) +
                         ", 5, 0, 9, 'x', 'MARKED', 1, '2026-01-01 00:00:00', NULL, NULL)"),
               DatabaseError);
  // foreign keys are on
  EXPECT_THROW(w.db.exec("INSERT INTO enrolment VALUES (999, 999, '2026-01-01 00:00:00')"),
               DatabaseError);
  // released without a release time
  EXPECT_THROW(w.db.exec("INSERT INTO mark VALUES (" + std::to_string(sub) +
                         ", 5, 0, 5, 'x', 'RELEASED', 1, '2026-01-01 00:00:00', NULL, NULL)"),
               DatabaseError);
}

TEST(Database, BadPathIsAReportedError) {
  EXPECT_THROW(Database("/no/such/dir/lms.db"), DatabaseError);
}

TEST(Database, ReopeningAFileKeepsTheData) {
  std::string path = ::testing::TempDir() + "reopen.db";
  std::remove(path.c_str());
  {
    Database db(path);
    SqliteRepository repo(db);
    repo.addDepartment("Kept");
  }
  Database db(path);
  SqliteRepository repo(db);
  EXPECT_TRUE(repo.departmentExists(1));
}


#include "lms/demo_data.hpp"

TEST(DemoData, LoadsAWorkingSchoolAndAdminCanLogIn) {
  setHashIterations(1000);
  Database db(":memory:");
  SqliteRepository repo(db);
  loadDemoData(repo);
  AuthService auth(repo, systemNow);
  EXPECT_EQ(auth.login("admin", "Admin#2026")->role(), Role::kAdmin);
  EXPECT_EQ(auth.login("hod1", "Hod#2026pw")->role(), Role::kHod);
  Statement st(db, "SELECT count(*) FROM submission");
  st.step();
  EXPECT_EQ(st.getInt(0), 2);
}
