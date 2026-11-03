#pragma once

#include <optional>
#include <string>

#include "lms/types.hpp"

namespace lms {

struct Assignment {
  int id = 0;
  int courseId = 0;
  int createdBy = 0;
  std::string title;
  std::string description;
  Time deadline = 0;
  int maxMark = 0;
};

struct Extension {
  int assignmentId = 0;
  int studentId = 0;
  Time newDeadline = 0;
  std::string reason;
  int grantedBy = 0;
  Time grantedAt = 0;
};

struct Submission {
  int id = 0;
  int assignmentId = 0;
  int studentId = 0;
  Time submittedAt = 0;
  std::string content;
};

/// Everything stored for one mark.
struct MarkData {
  int submissionId = 0;
  double rawMark = 0;
  int daysLate = 0;
  double finalMark = 0;
  std::string feedback;
  MarkStatus status = MarkStatus::kMarked;
  int markedBy = 0;
  Time markedAt = 0;
  std::optional<int> moderatedBy;
  std::optional<Time> releasedAt;
};

/// A mark and the moderation rules that protect it (BR3). Each method checks
/// the current status first, so a mark can never skip a step.
class Mark {
 public:
  explicit Mark(MarkData data) : d_(std::move(data)) {}
  const MarkData& data() const { return d_; }

  /// Lecturer changes a mark that is not approved yet (FR-13).
  void amend(double raw, double final, const std::string& feedback,
             int lecturerId, Time now);
  /// HoD approves as it is (FR-15).
  void approve(int hodId);
  /// HoD changes the mark and approves it in one go (FR-15).
  void adjust(int hodId, double raw, double final);
  /// HoD sends it back to the lecturer (FR-15).
  void returnForRemark(int hodId);
  /// Only an approved or adjusted mark can be released (BR3, FR-16).
  void release(Time now);

 private:
  MarkData d_;
};

}  // namespace lms
