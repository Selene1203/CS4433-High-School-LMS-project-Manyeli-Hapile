#include <gtest/gtest.h>

#include "lms/error.hpp"
#include "test_world.hpp"

using namespace lms;
using lms::testing::World;

// CR1: the lecturer extends one assignment for the whole class in one step.

TEST(ClassExtension, EveryEnrolledStudentGetsTheNewDeadline) {
  World w;
  Time ext = w.deadline + 2 * kSecondsPerDay;
  int n = w.assignments.extendForClass(*w.lecturer, w.assignment, ext, "power cut");
  EXPECT_EQ(n, 2);
  EXPECT_EQ(w.repo.effectiveDeadline(w.assignment, w.student->id()), ext);
  EXPECT_EQ(w.repo.effectiveDeadline(w.assignment, w.student2->id()), ext);
}

TEST(ClassExtension, StudentsCanNowSubmitOnTimeInsideTheExtension) {
  World w;
  w.assignments.extendForClass(*w.lecturer, w.assignment,
                               w.deadline + 2 * kSecondsPerDay, "power cut");
  w.now = w.deadline + kSecondsPerDay;
  EXPECT_FALSE(w.submissions.submit(*w.student, w.assignment, "w", false).late);
  EXPECT_FALSE(w.submissions.submit(*w.student2, w.assignment, "w", false).late);
}

TEST(ClassExtension, PenaltyIsWorkedOutFromTheExtendedDeadline) {
  World w;
  w.assignments.extendForClass(*w.lecturer, w.assignment,
                               w.deadline + 2 * kSecondsPerDay, "power cut");
  w.now = w.deadline + 2 * kSecondsPerDay + 3600;  // one hour after the extension
  int sub = w.submissions.submit(*w.student, w.assignment, "w", true).submissionId;
  EXPECT_EQ(w.marking.mark(*w.lecturer, sub, 15, "ok").daysLate, 1);
}

TEST(ClassExtension, EveryStudentGetsItsOwnAuditEntry) {  // FR-19
  World w;
  w.assignments.extendForClass(*w.lecturer, w.assignment,
                               w.deadline + kSecondsPerDay, "power cut");
  auto log = w.repo.listEntries();
  ASSERT_EQ(log.size(), 2u);
  for (const auto& e : log) {
    EXPECT_EQ(e.action, AuditAction::kExtend);
    EXPECT_EQ(e.reason, "power cut");
    EXPECT_EQ(e.actorId, w.lecturer->id());
  }
  EXPECT_NE(log[0].extStudentId, log[1].extStudentId);
}

TEST(ClassExtension, SkipsStudentsWhoseWorkIsAlreadyMarked) {  // BR4
  World w;
  int sub = w.submitOnTime(*w.student);
  w.marking.mark(*w.lecturer, sub, 10, "ok");
  Time ext = w.deadline + kSecondsPerDay;
  EXPECT_EQ(w.assignments.extendForClass(*w.lecturer, w.assignment, ext, "r"), 1);
  EXPECT_FALSE(w.repo.findExtension(w.assignment, w.student->id()).has_value());
  EXPECT_TRUE(w.repo.findExtension(w.assignment, w.student2->id()).has_value());
}

TEST(ClassExtension, NeverMakesAnExistingLaterDeadlineEarlier) {
  World w;
  Time later = w.deadline + 5 * kSecondsPerDay;
  w.assignments.grantExtension(*w.lecturer, w.assignment, w.student->id(), later, "ill");
  Time ext = w.deadline + 2 * kSecondsPerDay;
  EXPECT_EQ(w.assignments.extendForClass(*w.lecturer, w.assignment, ext, "power cut"), 1);
  EXPECT_EQ(w.repo.effectiveDeadline(w.assignment, w.student->id()), later);
  EXPECT_EQ(w.repo.effectiveDeadline(w.assignment, w.student2->id()), ext);
}

TEST(ClassExtension, NewDeadlineMustBeLaterThanTheAssignmentDeadline) {
  World w;
  EXPECT_THROW(w.assignments.extendForClass(*w.lecturer, w.assignment, w.deadline, "r"),
               ValidationError);
  EXPECT_THROW(w.assignments.extendForClass(*w.lecturer, w.assignment, w.deadline - 5, "r"),
               ValidationError);
}

TEST(ClassExtension, NeedsAReasonAndTheRightLecturer) {
  World w;
  Time ext = w.deadline + kSecondsPerDay;
  EXPECT_THROW(w.assignments.extendForClass(*w.lecturer, w.assignment, ext, ""),
               ValidationError);
  EXPECT_THROW(w.assignments.extendForClass(*w.student, w.assignment, ext, "r"),
               PermissionError);
  EXPECT_THROW(w.assignments.extendForClass(*w.hodUser, w.assignment, ext, "r"),
               PermissionError);
  EXPECT_THROW(w.assignments.extendForClass(*w.lecturer, 999, ext, "r"), NotFoundError);
  int other = w.admin.createUser(*w.adminUser, Role::kLecturer, "lec2", "Other", "Passw0rd!");
  auto lec2 = UserFactory::fromRecord(*w.repo.findUser(other));
  EXPECT_THROW(w.assignments.extendForClass(*lec2, w.assignment, ext, "r"), PermissionError);
  EXPECT_TRUE(w.repo.listEntries().empty());  // nothing was written
}

TEST(ClassExtension, SecondCallWithTheSameDeadlineChangesNothing) {
  World w;
  Time ext = w.deadline + kSecondsPerDay;
  w.assignments.extendForClass(*w.lecturer, w.assignment, ext, "r");
  EXPECT_THROW(w.assignments.extendForClass(*w.lecturer, w.assignment, ext, "r"), RuleError);
  EXPECT_EQ(w.repo.listEntries().size(), 2u);
}

TEST(ClassExtension, FailureHalfwayLeavesNoExtensionsBehind) {  // all or nothing
  World w;
  w.db.exec("ALTER TABLE audit_log RENAME TO audit_old");
  EXPECT_THROW(w.assignments.extendForClass(*w.lecturer, w.assignment,
                                            w.deadline + kSecondsPerDay, "r"),
               DatabaseError);
  EXPECT_FALSE(w.repo.findExtension(w.assignment, w.student->id()).has_value());
  EXPECT_FALSE(w.repo.findExtension(w.assignment, w.student2->id()).has_value());
}
