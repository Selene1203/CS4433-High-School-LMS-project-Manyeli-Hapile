#include <gtest/gtest.h>

#include "lms/domain.hpp"
#include "lms/error.hpp"

using namespace lms;

namespace {
Mark fresh() {
  MarkData d;
  d.submissionId = 1;
  d.rawMark = 15;
  d.finalMark = 15;
  d.feedback = "ok";
  d.markedBy = 5;
  d.markedAt = 100;
  return Mark(d);
}
}  // namespace

TEST(MarkStateMachine, HappyPathMarkedModeratedReleased) {
  Mark m = fresh();
  EXPECT_EQ(m.data().status, MarkStatus::kMarked);
  m.approve(9);
  EXPECT_EQ(m.data().status, MarkStatus::kModerated);
  EXPECT_EQ(m.data().moderatedBy, 9);
  m.release(200);
  EXPECT_EQ(m.data().status, MarkStatus::kReleased);
  EXPECT_EQ(m.data().releasedAt, 200);
}

TEST(MarkStateMachine, CannotReleaseWithoutApproval) {  // BR3
  Mark m = fresh();
  EXPECT_THROW(m.release(200), RuleError);
}

TEST(MarkStateMachine, ReturnedMarkGoesBackToMarkedWhenAmended) {
  Mark m = fresh();
  m.returnForRemark(9);
  EXPECT_EQ(m.data().status, MarkStatus::kReturned);
  EXPECT_THROW(m.approve(9), RuleError);
  m.amend(12, 12, "fixed", 5, 300);
  EXPECT_EQ(m.data().status, MarkStatus::kMarked);
  EXPECT_DOUBLE_EQ(m.data().rawMark, 12);
}

TEST(MarkStateMachine, AdjustApprovesAtTheNewValue) {
  Mark m = fresh();
  m.adjust(9, 18, 18);
  EXPECT_EQ(m.data().status, MarkStatus::kModerated);
  EXPECT_DOUBLE_EQ(m.data().finalMark, 18);
}

TEST(MarkStateMachine, ApprovedMarkCannotBeAmendedReturnedOrAdjusted) {
  Mark m = fresh();
  m.approve(9);
  EXPECT_THROW(m.amend(1, 1, "x", 5, 1), RuleError);
  EXPECT_THROW(m.returnForRemark(9), RuleError);
  EXPECT_THROW(m.adjust(9, 1, 1), RuleError);
  EXPECT_THROW(m.approve(9), RuleError);
}

TEST(MarkStateMachine, ReleasedMarkIsFinal) {
  Mark m = fresh();
  m.approve(9);
  m.release(1);
  EXPECT_THROW(m.release(2), RuleError);
  EXPECT_THROW(m.amend(1, 1, "x", 5, 1), RuleError);
  EXPECT_THROW(m.approve(9), RuleError);
}
