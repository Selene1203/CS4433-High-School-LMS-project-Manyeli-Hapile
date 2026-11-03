#include <gtest/gtest.h>

#include "lms/error.hpp"
#include "lms/types.hpp"

using namespace lms;

TEST(Time, FormatAndParseRoundTrip) {
  Time t = parseTime("2026-11-30 17:45");
  EXPECT_EQ(formatTime(t), "2026-11-30 17:45:00");
  EXPECT_EQ(parseTime(formatTime(t)), t);
}

TEST(Time, RejectsBadDates) {
  EXPECT_THROW(parseTime("tomorrow"), ValidationError);
  EXPECT_THROW(parseTime(""), ValidationError);
  EXPECT_THROW(parseTime("2026-13-01 10:00"), ValidationError);
  EXPECT_THROW(parseTime("2026-02-30 10:00"), ValidationError);
  EXPECT_THROW(parseTime("2026-11-30 25:00"), ValidationError);
}

TEST(Enums, RoleAndStatusRoundTrip) {
  for (Role r : {Role::kAdmin, Role::kLecturer, Role::kHod, Role::kStudent}) {
    EXPECT_EQ(parseRole(toString(r)), r);
  }
  for (MarkStatus s : {MarkStatus::kMarked, MarkStatus::kReturned,
                       MarkStatus::kModerated, MarkStatus::kReleased}) {
    EXPECT_EQ(parseMarkStatus(toString(s)), s);
  }
  EXPECT_THROW(parseRole("PRINCIPAL"), ValidationError);
  EXPECT_THROW(parseMarkStatus("DONE"), ValidationError);
}

TEST(Mark, FormattingDropsTrailingZeros) {
  EXPECT_EQ(formatMark(11.0), "11");
  EXPECT_EQ(formatMark(11.5), "11.5");
  EXPECT_EQ(formatMark(0.0), "0");
}
