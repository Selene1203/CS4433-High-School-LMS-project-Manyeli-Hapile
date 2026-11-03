#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include "lms/error.hpp"
#include "test_world.hpp"

using namespace lms;
using lms::testing::World;

TEST(Login, CorrectPasswordGivesTheRightRole) {
  World w;
  auto u = w.auth.login("lec", "Lec#12345");
  EXPECT_EQ(u->role(), Role::kLecturer);
}

TEST(Login, HodGetsTheirDepartment) {
  World w;
  auto u = w.auth.login("hod", "Hod#12345");
  EXPECT_EQ(static_cast<HeadOfDepartment&>(*u).deptId(), w.dept);
}

TEST(Login, WrongPasswordAndUnknownUserGiveTheSameMessage) {
  World w;
  std::string a, b;
  try { w.auth.login("lec", "nope"); } catch (const Error& e) { a = e.what(); }
  try { w.auth.login("ghost", "nope"); } catch (const Error& e) { b = e.what(); }
  EXPECT_EQ(a, b);
  EXPECT_FALSE(a.empty());
}

TEST(Login, FifthWrongPasswordLocksTheAccount) {  // FR-01
  World w;
  for (int i = 0; i < 4; ++i) {
    EXPECT_THROW(w.auth.login("stu1", "bad"), ValidationError);
  }
  EXPECT_THROW(w.auth.login("stu1", "bad"), RuleError);  // 5th
  EXPECT_THROW(w.auth.login("stu1", "Stu#12345"), RuleError);  // right but locked
}

TEST(Login, FourWrongThenRightResetsTheCounter) {
  World w;
  for (int i = 0; i < 4; ++i) EXPECT_THROW(w.auth.login("stu1", "bad"), Error);
  EXPECT_NO_THROW(w.auth.login("stu1", "Stu#12345"));
  for (int i = 0; i < 4; ++i) EXPECT_THROW(w.auth.login("stu1", "bad"), Error);
  EXPECT_NO_THROW(w.auth.login("stu1", "Stu#12345"));
}

TEST(Login, AdminResetUnlocksAndSetsNewPassword) {
  World w;
  for (int i = 0; i < 5; ++i) EXPECT_THROW(w.auth.login("stu1", "bad"), Error);
  w.admin.resetPassword(*w.adminUser, w.student->id(), "Fresh#12345");
  EXPECT_NO_THROW(w.auth.login("stu1", "Fresh#12345"));
}

TEST(Login, DeactivatedAccountCannotLogIn) {
  World w;
  w.admin.setActive(*w.adminUser, w.student->id(), false);
  EXPECT_THROW(w.auth.login("stu1", "Stu#12345"), RuleError);
  w.admin.setActive(*w.adminUser, w.student->id(), true);
  EXPECT_NO_THROW(w.auth.login("stu1", "Stu#12345"));
}

TEST(Login, PasswordIsNotStoredInThePlain) {  // NFR-03
  World w;
  for (const auto& u : w.repo.listUsers()) {
    EXPECT_EQ(u.passwordHash.rfind("pbkdf2$", 0), 0u);
    EXPECT_EQ(u.passwordHash.find("12345"), std::string::npos);
  }
}

TEST(ChangePassword, NeedsTheOldPasswordAndAValidNewOne) {  // FR-03
  World w;
  EXPECT_THROW(w.auth.changePassword(*w.student, "wrong", "Newpass#123"),
               ValidationError);
  EXPECT_THROW(w.auth.changePassword(*w.student, "Stu#12345", "short"),
               ValidationError);
  w.auth.changePassword(*w.student, "Stu#12345", "Newpass#123");
  EXPECT_NO_THROW(w.auth.login("stu1", "Newpass#123"));
  EXPECT_THROW(w.auth.login("stu1", "Stu#12345"), Error);
}

TEST(Admin, OnlyAnAdministratorCanManageAccountsAndCourses) {  // FR-02
  World w;
  EXPECT_THROW(w.admin.createUser(*w.lecturer, Role::kStudent, "x", "X", "Passw0rd!"),
               PermissionError);
  EXPECT_THROW(w.admin.createDepartment(*w.student, "Arts"), PermissionError);
  EXPECT_THROW(w.admin.createCourse(*w.hodUser, w.dept, "X1", "X"), PermissionError);
  EXPECT_THROW(w.admin.enrol(*w.lecturer, w.student->id(), w.course), PermissionError);
  EXPECT_THROW(w.admin.auditLog(*w.hodUser), PermissionError);
  EXPECT_THROW(w.admin.listUsers(*w.student), PermissionError);
}

TEST(Admin, DuplicateUsernameAndBadReferencesAreRefused) {
  World w;
  EXPECT_THROW(w.admin.createUser(*w.adminUser, Role::kStudent, "stu1", "Dup", "Passw0rd!"),
               ValidationError);
  EXPECT_THROW(w.admin.createCourse(*w.adminUser, 999, "Z1", "Z"), NotFoundError);
  EXPECT_THROW(w.admin.createCourse(*w.adminUser, w.dept, "CS101", "Again"), Error);
  EXPECT_THROW(w.admin.assignLecturer(*w.adminUser, w.course, w.student->id()), RuleError);
  EXPECT_THROW(w.admin.assignHod(*w.adminUser, w.dept, w.student->id()), RuleError);
  EXPECT_THROW(w.admin.enrol(*w.adminUser, w.lecturer->id(), w.course), RuleError);
  EXPECT_THROW(w.admin.enrol(*w.adminUser, w.student->id(), w.course), Error);  // twice
}

TEST(Admin, CannotDeactivateOrAnonymiseThemselves) {
  World w;
  EXPECT_THROW(w.admin.setActive(*w.adminUser, w.adminUser->id(), false), RuleError);
  EXPECT_THROW(w.admin.anonymiseUser(*w.adminUser, w.adminUser->id()), RuleError);
}

TEST(Admin, AnonymisingRemovesDetailsButKeepsTheMarks) {  // NFR-10
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 15, "good");
  w.admin.anonymiseUser(*w.adminUser, w.student->id());
  auto rec = w.repo.findUser(w.student->id());
  EXPECT_EQ(rec->fullName, "Former user");
  EXPECT_EQ(rec->username.rfind("deleted-", 0), 0u);
  EXPECT_FALSE(rec->active);
  EXPECT_TRUE(w.repo.findMark(sub).has_value());
  EXPECT_THROW(w.auth.login(rec->username, "Stu#12345"), Error);
}

TEST(Admin, AuditExportWritesOneLinePerEntry) {  // FR-21
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 15, "good");
  std::string path = ::testing::TempDir() + "audit_export.txt";
  w.admin.exportAudit(*w.adminUser, path);
  std::ifstream in(path);
  std::string line;
  int lines = 0;
  while (std::getline(in, line)) ++lines;
  EXPECT_EQ(lines, 1);
  EXPECT_THROW(w.admin.exportAudit(*w.adminUser, "/no/such/dir/x.txt"), Error);
}

TEST(Admin, BackupAndRestoreBringTheDataBack) {  // FR-22
  World w;
  std::string path = ::testing::TempDir() + "backup.db";
  std::remove(path.c_str());
  w.db.backupTo(path);
  w.admin.createUser(*w.adminUser, Role::kStudent, "extra", "Extra", "Passw0rd!");
  ASSERT_TRUE(w.repo.findUserByName("extra").has_value());
  w.db.restoreFrom(path);
  EXPECT_FALSE(w.repo.findUserByName("extra").has_value());
  EXPECT_TRUE(w.repo.findUserByName("stu1").has_value());
  EXPECT_THROW(w.db.restoreFrom("/no/such/file.db"), DatabaseError);
}
