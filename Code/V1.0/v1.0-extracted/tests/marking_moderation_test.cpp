#include <gtest/gtest.h>

#include <cmath>

#include "lms/error.hpp"
#include "test_world.hpp"

using namespace lms;
using lms::testing::World;

TEST(Marking, OnTimeWorkHasNoPenalty) {  // FR-12
  World w;
  int sub = w.submitOnTime(*w.student);
  MarkData m = w.marking.mark(*w.lecturer, sub, 15, "good");
  EXPECT_EQ(m.status, MarkStatus::kMarked);
  EXPECT_EQ(m.daysLate, 0);
  EXPECT_DOUBLE_EQ(m.finalMark, 15);
}

TEST(Marking, LateWorkGetsThePenaltyFromBR1) {  // FR-11
  World w;
  int sub = w.submitLate(*w.student, 30 * 3600);  // 30 hours, 2 days
  MarkData m = w.marking.mark(*w.lecturer, sub, 15, "good");
  EXPECT_EQ(m.daysLate, 2);
  EXPECT_DOUBLE_EQ(m.finalMark, 11);
  EXPECT_DOUBLE_EQ(m.rawMark, 15);
}

TEST(Marking, MoreThanThreeDaysLateGivesZero) {
  World w;
  int sub = w.submitLate(*w.student, 3 * kSecondsPerDay + 1);
  MarkData m = w.marking.mark(*w.lecturer, sub, 18, "late");
  EXPECT_EQ(m.daysLate, 4);
  EXPECT_DOUBLE_EQ(m.finalMark, 0);
}

TEST(Marking, ExtensionIsUsedForThePenalty) {
  World w;
  w.assignments.grantExtension(*w.lecturer, w.assignment, w.student->id(),
                               w.deadline + 2 * kSecondsPerDay, "ill");
  w.now = w.deadline + kSecondsPerDay;
  int sub = w.submissions.submit(*w.student, w.assignment, "w", false).submissionId;
  EXPECT_EQ(w.marking.mark(*w.lecturer, sub, 10, "ok").daysLate, 0);
}

TEST(Marking, ValidationAndPermissions) {
  World w;
  int sub = w.submitOnTime(*w.student);
  EXPECT_THROW(w.marking.mark(*w.lecturer, sub, -1, "x"), ValidationError);
  EXPECT_THROW(w.marking.mark(*w.lecturer, sub, 21, "x"), ValidationError);
  EXPECT_THROW(w.marking.mark(*w.lecturer, sub, std::nan(""), "x"), ValidationError);
  EXPECT_THROW(w.marking.mark(*w.lecturer, sub, 10, ""), ValidationError);
  EXPECT_THROW(w.marking.mark(*w.lecturer, 999, 10, "x"), NotFoundError);
  EXPECT_THROW(w.marking.mark(*w.student, sub, 10, "x"), PermissionError);
  EXPECT_THROW(w.marking.mark(*w.hodUser, sub, 10, "x"), PermissionError);
  EXPECT_FALSE(w.repo.findMark(sub).has_value());  // nothing was saved
}

TEST(Marking, LecturerOfAnotherCourseCannotMark) {
  World w;
  int sub = w.submitOnTime(*w.student);
  int other = w.admin.createUser(*w.adminUser, Role::kLecturer, "lec2", "Other", "Passw0rd!");
  auto lec2 = UserFactory::fromRecord(*w.repo.findUser(other));
  EXPECT_THROW(w.marking.mark(*lec2, sub, 10, "x"), PermissionError);
}

TEST(Marking, ChangingAMarkKeepsTheOldValueInTheAuditLog) {  // FR-13, FR-19
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 12, "first");
  w.marking.mark(*w.lecturer, sub, 16, "second look");
  auto log = w.repo.listEntries();
  ASSERT_EQ(log.size(), 2u);
  EXPECT_EQ(log[0].action, AuditAction::kMark);
  EXPECT_EQ(log[1].action, AuditAction::kAmend);
  EXPECT_NE(log[1].oldValue.find("raw 12"), std::string::npos);
  EXPECT_NE(log[1].newValue.find("raw 16"), std::string::npos);
  EXPECT_EQ(w.repo.findMark(sub)->rawMark, 16);
}

TEST(Marking, ApprovedMarkCannotBeChanged) {
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 12, "first");
  w.moderation.approve(*w.hodUser, sub);
  EXPECT_THROW(w.marking.mark(*w.lecturer, sub, 20, "sneaky"), RuleError);
  EXPECT_DOUBLE_EQ(w.repo.findMark(sub)->rawMark, 12);
  EXPECT_EQ(w.repo.listEntries().size(), 2u);  // mark + approve, no failed amend
}

TEST(Marking, UnmarkedListShrinksAsWorkIsMarked) {
  World w;
  int s1 = w.submitOnTime(*w.student);
  w.submitOnTime(*w.student2);
  EXPECT_EQ(w.marking.unmarked(*w.lecturer).size(), 2u);
  w.marking.mark(*w.lecturer, s1, 10, "ok");
  EXPECT_EQ(w.marking.unmarked(*w.lecturer).size(), 1u);
}

// ------------------------------------------------------------ moderation
TEST(Moderation, HodSeesMarkedWorkOfTheirDepartment) {  // FR-14
  World w;
  w.marking.mark(*w.lecturer, w.submitOnTime(*w.student), 10, "ok");
  EXPECT_EQ(w.moderation.listPending(*w.hodUser).size(), 1u);
  EXPECT_THROW(w.moderation.listPending(*w.lecturer), PermissionError);
}

TEST(Moderation, ApproveThenReleaseShowsTheMarkToTheStudent) {  // FR-15..17
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 15, "good");
  EXPECT_TRUE(w.submissions.releasedMarks(*w.student).empty());
  w.moderation.approve(*w.hodUser, sub);
  EXPECT_TRUE(w.submissions.releasedMarks(*w.student).empty());  // approved only
  w.moderation.release(*w.hodUser, sub);
  auto rows = w.submissions.releasedMarks(*w.student);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_DOUBLE_EQ(rows[0].mark.finalMark, 15);
  EXPECT_EQ(w.submissions.assignmentsFor(*w.student)[0].status, "MARK RELEASED");
}

TEST(Moderation, CannotReleaseAMarkThatIsNotApproved) {  // BR3
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 15, "good");
  EXPECT_THROW(w.moderation.release(*w.hodUser, sub), RuleError);
  EXPECT_TRUE(w.submissions.releasedMarks(*w.student).empty());
}

TEST(Moderation, AdjustNeedsAReasonAndKeepsThePenalty) {
  World w;
  int sub = w.submitLate(*w.student, 30 * 3600);  // 2 days late
  w.marking.mark(*w.lecturer, sub, 15, "good");
  EXPECT_THROW(w.moderation.adjust(*w.hodUser, sub, 18, ""), ValidationError);
  EXPECT_THROW(w.moderation.adjust(*w.hodUser, sub, 25, "too high"), ValidationError);
  w.moderation.adjust(*w.hodUser, sub, 18, "rubric applied wrongly");
  auto m = w.repo.findMark(sub);
  EXPECT_DOUBLE_EQ(m->rawMark, 18);
  EXPECT_DOUBLE_EQ(m->finalMark, 14);  // 18 minus 4 for two days
  EXPECT_EQ(m->status, MarkStatus::kModerated);
}

TEST(Moderation, ReturnedMarkGoesBackToTheLecturerAndCanBeRemarked) {  // FR-13, FR-15
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 5, "harsh");
  EXPECT_THROW(w.moderation.returnMark(*w.hodUser, sub, ""), ValidationError);
  w.moderation.returnMark(*w.hodUser, sub, "please check question 3");
  EXPECT_EQ(w.marking.returned(*w.lecturer).size(), 1u);
  EXPECT_TRUE(w.moderation.listPending(*w.hodUser).empty());
  w.marking.mark(*w.lecturer, sub, 9, "re-marked");
  EXPECT_EQ(w.repo.findMark(sub)->status, MarkStatus::kMarked);
  EXPECT_EQ(w.moderation.listPending(*w.hodUser).size(), 1u);
}

TEST(Moderation, HodOfAnotherDepartmentIsRefused) {  // A4
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 15, "good");
  int d2 = w.admin.createDepartment(*w.adminUser, "Arts");
  int h2 = w.admin.createUser(*w.adminUser, Role::kHod, "hod2", "Other Head", "Passw0rd!");
  w.admin.assignHod(*w.adminUser, d2, h2);
  auto hod2 = UserFactory::fromRecord(*w.repo.findUser(h2), d2);
  EXPECT_THROW(w.moderation.approve(*hod2, sub), PermissionError);
  EXPECT_TRUE(w.moderation.listPending(*hod2).empty());
}

TEST(Moderation, HodWithoutADepartmentCannotModerate) {
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 15, "good");
  int h = w.admin.createUser(*w.adminUser, Role::kHod, "hod3", "No Dept", "Passw0rd!");
  auto hod = UserFactory::fromRecord(*w.repo.findUser(h), 0);
  EXPECT_THROW(w.moderation.approve(*hod, sub), PermissionError);
  EXPECT_TRUE(w.moderation.listPending(*hod).empty());
}

TEST(Moderation, HodCannotModerateTheirOwnMarks) {  // A4
  World w;
  int sub = w.submitOnTime(*w.student);
  // the HoD also teaches the course and marks it
  w.repo.addLecturer(w.course, w.hodUser->id());
  // role check blocks a HoD marking, so write the mark as stored data
  MarkData d;
  d.submissionId = sub;
  d.rawMark = 10;
  d.finalMark = 10;
  d.feedback = "x";
  d.markedBy = w.hodUser->id();
  d.markedAt = w.now;
  w.repo.saveMark(d);
  EXPECT_THROW(w.moderation.approve(*w.hodUser, sub), PermissionError);
}

TEST(Moderation, MissingMarkAndWrongRole) {
  World w;
  int sub = w.submitOnTime(*w.student);
  EXPECT_THROW(w.moderation.approve(*w.hodUser, sub), NotFoundError);
  EXPECT_THROW(w.moderation.approve(*w.hodUser, 999), NotFoundError);
  EXPECT_THROW(w.moderation.approve(*w.lecturer, sub), PermissionError);
  EXPECT_THROW(w.moderation.release(*w.student, sub), PermissionError);
}

TEST(Moderation, EveryStepIsAudited) {  // FR-19
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 5, "harsh");
  w.moderation.returnMark(*w.hodUser, sub, "recheck");
  w.marking.mark(*w.lecturer, sub, 9, "again");
  w.moderation.adjust(*w.hodUser, sub, 10, "fair");
  w.moderation.release(*w.hodUser, sub);
  std::vector<AuditAction> got;
  for (const auto& e : w.repo.listEntries()) got.push_back(e.action);
  std::vector<AuditAction> want = {AuditAction::kMark, AuditAction::kReturn,
                                   AuditAction::kAmend, AuditAction::kAdjust,
                                   AuditAction::kRelease};
  EXPECT_EQ(got, want);
  for (const auto& e : w.repo.listEntries()) {
    EXPECT_GT(e.actorId, 0);
    EXPECT_GT(e.at, 0);
  }
}
