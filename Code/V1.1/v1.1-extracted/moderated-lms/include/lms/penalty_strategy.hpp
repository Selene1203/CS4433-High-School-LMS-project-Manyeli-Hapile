#pragma once

#include "lms/types.hpp"

namespace lms {

/// Late penalty policy (Strategy pattern). Returns how many marks come off.
class PenaltyStrategy {
 public:
  virtual ~PenaltyStrategy() = default;
  virtual double penalty(int daysLate, int maxMark) const = 0;
};

/// BR1: 10% of the maximum mark per day late, up to 3 days, after that the
/// whole mark is lost.
class StandardPenaltyStrategy final : public PenaltyStrategy {
 public:
  explicit StandardPenaltyStrategy(int percentPerDay = 10, int maxDays = 3)
      : percentPerDay_(percentPerDay), maxDays_(maxDays) {}
  double penalty(int daysLate, int maxMark) const override;

 private:
  int percentPerDay_;
  int maxDays_;
};

/// Hours late to whole days, rounded up. 0 or less means on time (BR1).
int daysLate(Time deadline, Time submittedAt);

/// Final mark never goes below 0.
double finalMark(double rawMark, double penalty);

}  // namespace lms
