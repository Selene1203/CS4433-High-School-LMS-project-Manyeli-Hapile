#include <gtest/gtest.h>

#include "lms/penalty_strategy.hpp"

using namespace lms;

// BR1 boundaries from the SRS: 1 second late is 1 day, exactly 24 hours is
// 1 day, 24 hours and 1 second is 2 days, 72 hours is 3 days and 72 hours and
// 1 second is zero.
TEST(DaysLate, OnTimeAndExactlyOnDeadlineAreZero) {
  EXPECT_EQ(daysLate(1000, 900), 0);
  EXPECT_EQ(daysLate(1000, 1000), 0);
}

TEST(DaysLate, OneSecondLateIsOneDay) { EXPECT_EQ(daysLate(1000, 1001), 1); }

TEST(DaysLate, Exactly24HoursIsOneDay) {
  EXPECT_EQ(daysLate(0, kSecondsPerDay), 1);
}

TEST(DaysLate, TwentyFourHoursAndOneSecondIsTwoDays) {
  EXPECT_EQ(daysLate(0, kSecondsPerDay + 1), 2);
}

TEST(DaysLate, Exactly72HoursIsThreeDays) {
  EXPECT_EQ(daysLate(0, 3 * kSecondsPerDay), 3);
}

TEST(DaysLate, SeventyTwoHoursAndOneSecondIsFourDays) {
  EXPECT_EQ(daysLate(0, 3 * kSecondsPerDay + 1), 4);
}

TEST(StandardPenalty, NoPenaltyWhenOnTime) {
  StandardPenaltyStrategy p;
  EXPECT_DOUBLE_EQ(p.penalty(0, 20), 0.0);
}

TEST(StandardPenalty, TenPercentOfMaximumPerDay) {
  StandardPenaltyStrategy p;
  EXPECT_DOUBLE_EQ(p.penalty(1, 20), 2.0);
  EXPECT_DOUBLE_EQ(p.penalty(2, 20), 4.0);
  EXPECT_DOUBLE_EQ(p.penalty(3, 20), 6.0);
}

TEST(StandardPenalty, MoreThanThreeDaysLosesEverything) {
  StandardPenaltyStrategy p;
  EXPECT_DOUBLE_EQ(p.penalty(4, 20), 20.0);
  EXPECT_DOUBLE_EQ(p.penalty(30, 50), 50.0);
}

TEST(StandardPenalty, CustomPolicyCanBeInjected) {
  StandardPenaltyStrategy p(5, 2);  // 5% per day, 2 days
  EXPECT_DOUBLE_EQ(p.penalty(2, 40), 4.0);
  EXPECT_DOUBLE_EQ(p.penalty(3, 40), 40.0);
}

TEST(FinalMark, WorkedExampleFromTheSrs) {
  // maximum 20, raw 15, 30 hours late: 2 days, penalty 4, final 11
  StandardPenaltyStrategy p;
  int days = daysLate(0, 30 * 3600);
  EXPECT_EQ(days, 2);
  EXPECT_DOUBLE_EQ(finalMark(15, p.penalty(days, 20)), 11.0);
}

TEST(FinalMark, NeverBelowZero) {
  EXPECT_DOUBLE_EQ(finalMark(3, 6), 0.0);
}
