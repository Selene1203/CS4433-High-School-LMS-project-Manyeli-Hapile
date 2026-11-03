#include "lms/domain.hpp"

#include "lms/error.hpp"

namespace lms {

void Mark::amend(double raw, double final, const std::string& feedback,
                 int lecturerId, Time now) {
  if (d_.status != MarkStatus::kMarked && d_.status != MarkStatus::kReturned) {
    throw RuleError("this mark has been approved, it can no longer be changed");
  }
  d_.rawMark = raw;
  d_.finalMark = final;
  d_.feedback = feedback;
  d_.markedBy = lecturerId;
  d_.markedAt = now;
  d_.status = MarkStatus::kMarked;
}

void Mark::approve(int hodId) {
  if (d_.status != MarkStatus::kMarked) {
    throw RuleError("only a marked submission can be approved");
  }
  d_.status = MarkStatus::kModerated;
  d_.moderatedBy = hodId;
}

void Mark::adjust(int hodId, double raw, double final) {
  if (d_.status != MarkStatus::kMarked) {
    throw RuleError("only a marked submission can be adjusted");
  }
  d_.rawMark = raw;
  d_.finalMark = final;
  d_.status = MarkStatus::kModerated;
  d_.moderatedBy = hodId;
}

void Mark::returnForRemark(int hodId) {
  if (d_.status != MarkStatus::kMarked) {
    throw RuleError("only a marked submission can be returned");
  }
  d_.status = MarkStatus::kReturned;
  d_.moderatedBy = hodId;
}

void Mark::release(Time now) {
  if (d_.status != MarkStatus::kModerated) {
    throw RuleError("a mark must be approved before it is released");
  }
  d_.status = MarkStatus::kReleased;
  d_.releasedAt = now;
}

}  // namespace lms
