#include <gtest/gtest.h>

#include <sstream>

#include "lms/console_ui.hpp"
#include "test_world.hpp"

using namespace lms;
using lms::testing::World;

namespace {

std::string drive(World& w, const std::string& script) {
  std::istringstream in(script);
  std::ostringstream out;
  Services s{w.auth, w.admin, w.assignments, w.submissions, w.marking,
             w.moderation, w.db, ":memory:"};
  ConsoleUi ui(s, in, out);
  ui.run();
  return out.str();
}

}  // namespace

TEST(ConsoleUi, LeavingAtTheLoginPromptEndsTheProgram) {
  World w;
  EXPECT_NE(drive(w, "\n").find("Moderated LMS"), std::string::npos);
}

TEST(ConsoleUi, WrongPasswordShowsAMessageAndAsksAgain) {
  World w;
  std::string out = drive(w, "stu1\nbad\nstu1\nStu#12345\n0\n\n");
  EXPECT_NE(out.find("wrong username or password"), std::string::npos);
  EXPECT_NE(out.find("Welcome, Student One"), std::string::npos);
}

TEST(ConsoleUi, EachRoleOnlySeesItsOwnMenu) {  // NFR-07
  World w;
  std::string student = drive(w, "stu1\nStu#12345\n0\n\n");
  EXPECT_NE(student.find("Submit work"), std::string::npos);
  EXPECT_EQ(student.find("Create user"), std::string::npos);
  EXPECT_EQ(student.find("Approve a mark"), std::string::npos);
  std::string hod = drive(w, "hod\nHod#12345\n0\n\n");
  EXPECT_NE(hod.find("Approve a mark"), std::string::npos);
  EXPECT_EQ(hod.find("Submit work"), std::string::npos);
  std::string lec = drive(w, "lec\nLec#12345\n0\n\n");
  EXPECT_NE(lec.find("Mark a submission"), std::string::npos);
  std::string admin = drive(w, "root\nAdmin#1234\n0\n\n");
  EXPECT_NE(admin.find("Create user"), std::string::npos);
}

TEST(ConsoleUi, ThirtyBadInputsNeverCrashOrChangeData) {  // NFR-05
  World w;
  std::string script = "lec\nLec#12345\n";
  const char* bad[] = {"abc", "", "-5", "99999999999999", "1.5", "'; DROP--"};
  for (int i = 0; i < 5; ++i) {
    for (const char* b : bad) {
      script += std::string("4\n") + b + "\n" + b + "\n" + b + "\n";   // mark
      script += std::string("1\n") + b + "\n" + b + "\n\n" + b + "\n" + b + "\n"; // assignment
    }
  }
  script += "0\n\n";
  EXPECT_NO_THROW(drive(w, script));
  EXPECT_EQ(w.repo.listForCourse(w.course).size(), 1u);  // only the original
  Statement st(w.db, "SELECT count(*) FROM mark");
  st.step();
  EXPECT_EQ(st.getInt(0), 0);
}

TEST(ConsoleUi, MenuNumbersOutOfRangeAreAskedAgain) {
  World w;
  std::string out = drive(w, "stu1\nStu#12345\n9\nx\n-1\n0\n\n");
  EXPECT_NE(out.find("Please choose a number"), std::string::npos);
}

TEST(ConsoleUi, StudentCanSubmitAndSeeTheAssignment) {
  World w;
  std::string out = drive(w, "stu1\nStu#12345\n2\n" + std::to_string(w.assignment) +
                                 "\nmy answer\nEND\n1\n0\n\n");
  EXPECT_NE(out.find("Status SUBMITTED"), std::string::npos);
  EXPECT_NE(out.find("Lab report"), std::string::npos);
  EXPECT_EQ(w.repo.findSubmissionOf(w.assignment, w.student->id())->content, "my answer\n");
}

TEST(ConsoleUi, LateSubmissionShowsTheWarningAndNeedsYes) {
  World w;
  w.now = w.deadline + 3600;
  std::string a = std::to_string(w.assignment);
  std::string out = drive(w, "stu1\nStu#12345\n2\n" + a + "\nwork\nEND\nno\n0\n\n");
  EXPECT_NE(out.find("1 day(s) late"), std::string::npos);
  EXPECT_NE(out.find("Not submitted"), std::string::npos);
  EXPECT_FALSE(w.repo.findSubmissionOf(w.assignment, w.student->id()).has_value());
  out = drive(w, "stu1\nStu#12345\n2\n" + a + "\nwork\nEND\nYES\n0\n\n");
  EXPECT_NE(out.find("Status LATE"), std::string::npos);
}

TEST(ConsoleUi, FullModerationRoundThroughTheMenus) {  // end to end
  World w;
  std::string a = std::to_string(w.assignment);
  drive(w, "stu1\nStu#12345\n2\n" + a + "\nwork\nEND\n0\n\n");
  int sub = w.repo.findSubmissionOf(w.assignment, w.student->id())->id;
  std::string s = std::to_string(sub);
  drive(w, "lec\nLec#12345\n4\n" + s + "\n15\nnice\n0\n\n");
  EXPECT_EQ(w.repo.findMark(sub)->status, MarkStatus::kMarked);
  std::string out = drive(w, "hod\nHod#12345\n5\n" + s + "\n2\n" + s + "\n5\n" + s + "\n0\n\n");
  EXPECT_NE(out.find("a mark must be approved before it is released"), std::string::npos);
  EXPECT_EQ(w.repo.findMark(sub)->status, MarkStatus::kReleased);
  out = drive(w, "stu1\nStu#12345\n3\n0\n\n");
  EXPECT_NE(out.find("final 15"), std::string::npos);
}

TEST(ConsoleUi, StudentCannotReachAdminFeatures) {
  World w;
  // typing admin menu numbers into the student menu does nothing harmful
  std::string out = drive(w, "stu1\nStu#12345\n6\n7\n11\n0\n\n");
  EXPECT_EQ(w.repo.listUsers().size(), 5u);
  EXPECT_EQ(out.find("audit"), std::string::npos);
}

TEST(ConsoleUi, AdminMenuRoundTrip) {
  World w;
  std::string out = drive(
      w, "root\nAdmin#1234\n1\nSTUDENT\nnewkid\nNew Kid\nPassw0rd!\n2\n"
         "6\nArts\n11\n13\n" + ::testing::TempDir() + "ui_backup.db\n0\n\n");
  EXPECT_NE(out.find("Created user"), std::string::npos);
  EXPECT_NE(out.find("newkid"), std::string::npos);
  EXPECT_NE(out.find("Created department"), std::string::npos);
  EXPECT_NE(out.find("Backup written"), std::string::npos);
}

TEST(ConsoleUi, LecturerCanExtendTheWholeClassFromTheMenu) {  // CR1
  World w;
  std::string a = std::to_string(w.assignment);
  Time ext = w.deadline + kSecondsPerDay;
  std::string out = drive(w, "lec\nLec#12345\n7\n" + a + "\n" + formatTime(ext) +
                                 "\npower cut\n0\n\n");
  EXPECT_NE(out.find("Extension saved for 2 student(s)"), std::string::npos);
  EXPECT_EQ(w.repo.effectiveDeadline(w.assignment, w.student2->id()), ext);
}
