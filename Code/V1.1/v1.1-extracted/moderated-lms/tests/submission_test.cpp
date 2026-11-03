#include <gtest/gtest.h>

#include "lms/error.hpp"
#include "test_world.hpp"

using namespace lms;
using lms::testing::World;

TEST(Assignment, LecturerCreatesOnlyForTheirOwnCourse) {  // FR-07
  World w;
  EXPECT_THROW(w.assignments.createAssignment(*w.student, w.course, "T", "", w.deadline, 10),
               PermissionError);
  int other = w.admin.createCourse(*w.adminUser, w.dept, "CS102", "Other");
  EXPECT_THROW(w.assignments.createAssignment(*w.lecturer, other, "T", "", w.deadline, 10),
               PermissionError);
}

TEST(Assignment, RejectsBadInput) {
  World w;
  EXPECT_THROW(w.assignments.createAssignment(*w.lecturer, w.course, "", "", w.deadline, 10),
               ValidationError);
  EXPECT_THROW(w.assignments.createAssignment(*w.lecturer, w.course, "T", "", w.deadline, 0),
               ValidationError);
  EXPECT_THROW(w.assignments.createAssignment(*w.lecturer, w.course, "T", "", w.deadline, 5000),
               ValidationError);
  EXPECT_THROW(w.assignments.createAssignment(*w.lecturer, w.course, "T", "", w.now - 1, 10),
               ValidationError);
  EXPECT_THROW(w.assignments.createAssignment(*w.lecturer, 999, "T", "", w.deadline, 10),
               NotFoundError);
  EXPECT_THROW(w.assignments.createAssignment(*w.lecturer, w.course, "Lab report", "",
                                              w.deadline, 10),
               Error);  // same title twice in one course
}

TEST(Submit, OnTimeIsSubmittedNotLate) {  // FR-09
  World w;
  auto r = w.submissions.submit(*w.student, w.assignment, "my work", false);
  EXPECT_EQ(r.outcome, SubmitResult::Outcome::kSaved);
  EXPECT_FALSE(r.late);
  EXPECT_EQ(w.submissions.assignmentsFor(*w.student)[0].status, "SUBMITTED");
}

TEST(Submit, ExactlyOnTheDeadlineIsStillOnTime) {
  World w;
  w.now = w.deadline;
  EXPECT_FALSE(w.submissions.submit(*w.student, w.assignment, "x", false).late);
}

TEST(Submit, LateWorkAsksForConfirmationFirst) {
  World w;
  w.now = w.deadline + 3600;
  auto r = w.submissions.submit(*w.student, w.assignment, "work", false);
  EXPECT_EQ(r.outcome, SubmitResult::Outcome::kNeedsConfirmation);
  EXPECT_EQ(r.daysLate, 1);
  EXPECT_DOUBLE_EQ(r.penaltyOnMax, 2.0);
  EXPECT_FALSE(w.repo.findSubmissionOf(w.assignment, w.student->id()).has_value());
}

TEST(Submit, ConfirmedLateWorkIsSavedAndFlaggedLate) {
  World w;
  int sub = w.submitLate(*w.student, 3600);
  EXPECT_GT(sub, 0);
  EXPECT_EQ(w.submissions.assignmentsFor(*w.student)[0].status, "LATE");
}

TEST(Submit, ResubmittingBeforeTheDeadlineKeepsOnlyTheLatest) {  // FR-10
  World w;
  w.submissions.submit(*w.student, w.assignment, "first", false);
  w.now += 3600;
  auto r = w.submissions.submit(*w.student, w.assignment, "second", false);
  EXPECT_TRUE(r.replaced);
  auto s = w.repo.findSubmissionOf(w.assignment, w.student->id());
  EXPECT_EQ(s->content, "second");
  EXPECT_EQ(s->submittedAt, w.now);
}

TEST(Submit, CannotChangeWorkAfterTheDeadline) {  // BR2
  World w;
  w.submissions.submit(*w.student, w.assignment, "first", false);
  w.now = w.deadline + 1;
  EXPECT_THROW(w.submissions.submit(*w.student, w.assignment, "second", true),
               RuleError);
  EXPECT_EQ(w.repo.findSubmissionOf(w.assignment, w.student->id())->content, "first");
}

TEST(Submit, LateSubmissionCanOnlyBeMadeOnce) {
  World w;
  w.submitLate(*w.student, 100);
  EXPECT_THROW(w.submissions.submit(*w.student, w.assignment, "again", true), RuleError);
}

TEST(Submit, CannotChangeWorkThatIsAlreadyMarked) {
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 10, "ok");
  EXPECT_THROW(w.submissions.submit(*w.student, w.assignment, "new", false), RuleError);
}

TEST(Submit, ExtensionMovesTheEffectiveDeadline) {  // FR-08, BR2, BR4
  World w;
  Time ext = w.deadline + 2 * kSecondsPerDay;
  w.assignments.grantExtension(*w.lecturer, w.assignment, w.student->id(), ext, "ill");
  w.now = w.deadline + kSecondsPerDay;  // late for others, on time for stu1
  EXPECT_FALSE(w.submissions.submit(*w.student, w.assignment, "work", false).late);
  EXPECT_TRUE(w.submissions.submit(*w.student2, w.assignment, "work", false).outcome ==
              SubmitResult::Outcome::kNeedsConfirmation);
}

TEST(Submit, ValidationAndAccessRules) {  // NFR-05, FR-02
  World w;
  EXPECT_THROW(w.submissions.submit(*w.student, w.assignment, "", false), ValidationError);
  EXPECT_THROW(w.submissions.submit(*w.student, w.assignment, "  \n ", false), ValidationError);
  EXPECT_THROW(w.submissions.submit(*w.student, w.assignment, std::string(200000, 'a'), false),
               ValidationError);
  EXPECT_THROW(w.submissions.submit(*w.student, 999, "x", false), NotFoundError);
  EXPECT_THROW(w.submissions.submit(*w.lecturer, w.assignment, "x", false), PermissionError);
  auto outsider = UserFactory::fromRecord(
      *w.repo.findUser(w.admin.createUser(*w.adminUser, Role::kStudent, "out", "Out", "Passw0rd!")));
  EXPECT_THROW(w.submissions.submit(*outsider, w.assignment, "x", false), PermissionError);
}

TEST(Extension, RulesFromBR4) {
  World w;
  Time ext = w.deadline + kSecondsPerDay;
  EXPECT_THROW(w.assignments.grantExtension(*w.student, w.assignment, w.student->id(), ext, "r"),
               PermissionError);
  EXPECT_THROW(w.assignments.grantExtension(*w.lecturer, w.assignment, w.student->id(), ext, ""),
               ValidationError);
  EXPECT_THROW(w.assignments.grantExtension(*w.lecturer, w.assignment, w.student->id(),
                                            w.deadline - 1, "r"),
               ValidationError);
  EXPECT_THROW(w.assignments.grantExtension(*w.lecturer, 999, w.student->id(), ext, "r"),
               NotFoundError);
  // once the work is marked it is too late
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 10, "ok");
  EXPECT_THROW(w.assignments.grantExtension(*w.lecturer, w.assignment, w.student->id(), ext, "r"),
               RuleError);
}

TEST(Extension, WritesAnAuditEntry) {  // FR-19
  World w;
  w.assignments.grantExtension(*w.lecturer, w.assignment, w.student->id(),
                               w.deadline + kSecondsPerDay, "family event");
  auto log = w.repo.listEntries();
  ASSERT_EQ(log.size(), 1u);
  EXPECT_EQ(log[0].action, AuditAction::kExtend);
  EXPECT_EQ(log[0].reason, "family event");
  EXPECT_EQ(log[0].extStudentId, w.student->id());
}

TEST(StudentView, ListsAssignmentsWithDeadlinesAndStatus) {  // FR-18
  World w;
  auto v = w.submissions.assignmentsFor(*w.student);
  ASSERT_EQ(v.size(), 1u);
  EXPECT_EQ(v[0].status, "NOT SUBMITTED");
  EXPECT_EQ(v[0].effectiveDeadline, w.deadline);
  EXPECT_THROW(w.submissions.assignmentsFor(*w.lecturer), PermissionError);
}
