#include "lms/penalty_strategy.hpp"

#include <algorithm>

namespace lms {

double StandardPenaltyStrategy::penalty(int daysLate, int maxMark) const {
  if (daysLate <= 0) return 0.0;
  if (daysLate > maxDays_) return static_cast<double>(maxMark);
  return static_cast<double>(maxMark) * (percentPerDay_ * daysLate) / 100.0;
}

int daysLate(Time deadline, Time submittedAt) {
  if (submittedAt <= deadline) return 0;
  Time late = submittedAt - deadline;
  return static_cast<int>((late + kSecondsPerDay - 1) / kSecondsPerDay);
}

double finalMark(double rawMark, double penalty) {
  return std::max(0.0, rawMark - penalty);
}

}  // namespace lms
