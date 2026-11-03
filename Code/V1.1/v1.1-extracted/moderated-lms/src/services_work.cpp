#include <cmath>

#include "lms/error.hpp"
#include "lms/services.hpp"

namespace lms {
namespace {

constexpr std::size_t kMaxContent = 100000;
constexpr std::size_t kMaxText = 2000;
constexpr int kMaxMarkLimit = 1000;

std::string trim(const std::string& s) {
  std::size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  std::size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

void requireMarkInRange(double mark, int maxMark) {
  if (!std::isfinite(mark) || mark < 0 || mark > maxMark) {
    throw ValidationError("mark must be between 0 and " +
                          std::to_string(maxMark));
  }
}

std::string describe(const MarkData& m) {
  return "raw " + formatMark(m.rawMark) + ", final " + formatMark(m.finalMark) +
         ", " + toString(m.status);
}

}  // namespace

// ----------------------------------------------------------- assignments
int AssignmentService::createAssignment(const User& lecturer, int courseId,
                                        const std::string& title,
                                        const std::string& description,
                                        Time deadline, int maxMark) {
  requireRole(lecturer, Role::kLecturer);
  if (!courses_.findCourse(courseId)) throw NotFoundError("course not found");
  if (!courses_.isLecturerOf(lecturer.id(), courseId)) {
    throw PermissionError("you do not teach this course");
  }
  std::string t = trim(title);
  if (t.empty() || t.size() > 100) {
    throw ValidationError("title must be 1 to 100 characters");
  }
  if (description.size() > kMaxText) throw ValidationError("description too long");
  if (maxMark < 1 || maxMark > kMaxMarkLimit) {
    throw ValidationError("maximum mark must be between 1 and 1000");
  }
  if (deadline <= clock_()) throw ValidationError("deadline must be in the future");
  Assignment a;
  a.courseId = courseId;
  a.createdBy = lecturer.id();
  a.title = t;
  a.description = description;
  a.deadline = deadline;
  a.maxMark = maxMark;
  return assignments_.addAssignment(a);
}

void AssignmentService::grantExtension(const User& lecturer, int assignmentId,
                                       int studentId, Time newDeadline,
                                       const std::string& reason) {
  requireRole(lecturer, Role::kLecturer);
  auto a = assignments_.findAssignment(assignmentId);
  if (!a) throw NotFoundError("assignment not found");
  if (!courses_.isLecturerOf(lecturer.id(), a->courseId)) {
    throw PermissionError("you do not teach this course");
  }
  if (!courses_.isEnrolled(studentId, a->courseId)) {
    throw RuleError("that student is not enrolled in this course");
  }
  std::string why = trim(reason);
  if (why.empty() || why.size() > kMaxText) {
    throw ValidationError("an extension needs a reason");
  }
  // BR4: only until that student's work is marked.
  if (auto sub = submissions_.findSubmissionOf(assignmentId, studentId)) {
    if (marks_.findMark(sub->id)) {
      throw RuleError("this student's work is already marked");
    }
  }
  Time current = assignments_.effectiveDeadline(assignmentId, studentId);
  if (newDeadline <= current) {
    throw ValidationError("the new deadline must be later than the current one");
  }
  Transaction tx(uow_);
  writeExtension(lecturer.id(), assignmentId, studentId, current, newDeadline,
                 why);
  tx.commit();
}

void AssignmentService::writeExtension(int lecturerId, int assignmentId,
                                       int studentId, Time current,
                                       Time newDeadline,
                                       const std::string& reason) {
  Extension e;
  e.assignmentId = assignmentId;
  e.studentId = studentId;
  e.newDeadline = newDeadline;
  e.reason = reason;
  e.grantedBy = lecturerId;
  e.grantedAt = clock_();
  assignments_.saveExtension(e);
  AuditEntry entry;
  entry.actorId = lecturerId;
  entry.at = e.grantedAt;
  entry.action = AuditAction::kExtend;
  entry.extAssignmentId = assignmentId;
  entry.extStudentId = studentId;
  entry.oldValue = formatTime(current);
  entry.newValue = formatTime(newDeadline);
  entry.reason = reason;
  audit_.append(entry);
}

bool AssignmentService::classExtensionApplies(int assignmentId, int studentId,
                                              Time newDeadline, Time& current) {
  // BR4: not once that student's work is marked.
  if (auto sub = submissions_.findSubmissionOf(assignmentId, studentId)) {
    if (marks_.findMark(sub->id)) return false;
  }
  current = assignments_.effectiveDeadline(assignmentId, studentId);
  return newDeadline > current;  // never make a deadline earlier
}

int AssignmentService::extendForClass(const User& lecturer, int assignmentId,
                                      Time newDeadline,
                                      const std::string& reason) {
  requireRole(lecturer, Role::kLecturer);
  auto a = assignments_.findAssignment(assignmentId);
  if (!a) throw NotFoundError("assignment not found");
  if (!courses_.isLecturerOf(lecturer.id(), a->courseId)) {
    throw PermissionError("you do not teach this course");
  }
  std::string why = trim(reason);
  if (why.empty() || why.size() > kMaxText) {
    throw ValidationError("an extension needs a reason");
  }
  if (newDeadline <= a->deadline) {
    throw ValidationError(
        "the new deadline must be later than the assignment deadline");
  }
  Transaction tx(uow_);
  int changed = 0;
  for (int studentId : courses_.studentsOfCourse(a->courseId)) {
    Time current = 0;
    if (!classExtensionApplies(assignmentId, studentId, newDeadline, current)) {
      continue;
    }
    writeExtension(lecturer.id(), assignmentId, studentId, current, newDeadline,
                   why);
    ++changed;
  }
  if (changed == 0) {
    throw RuleError("no student needed this extension");
  }
  tx.commit();
  return changed;
}

std::vector<Assignment> AssignmentService::listForCourse(const User& lecturer,
                                                         int courseId) {
  requireRole(lecturer, Role::kLecturer);
  if (!courses_.isLecturerOf(lecturer.id(), courseId)) {
    throw PermissionError("you do not teach this course");
  }
  return assignments_.listForCourse(courseId);
}

// ------------------------------------------------------------ submitting
SubmitResult SubmissionService::submit(const User& student, int assignmentId,
                                       const std::string& content,
                                       bool confirmLate) {
  requireRole(student, Role::kStudent);
  if (trim(content).empty()) throw ValidationError("the work cannot be empty");
  if (content.size() > kMaxContent) throw ValidationError("the work is too long");
  auto a = assignments_.findAssignment(assignmentId);
  if (!a) throw NotFoundError("assignment not found");
  if (!courses_.isEnrolled(student.id(), a->courseId)) {
    throw PermissionError("you are not enrolled in this course");
  }
  Time now = clock_();
  Time deadline = assignments_.effectiveDeadline(assignmentId, student.id());
  int late = daysLate(deadline, now);
  SubmitResult r;
  r.late = late > 0;
  r.daysLate = late;
  r.penaltyOnMax = penalty_.penalty(late, a->maxMark);

  auto existing = submissions_.findSubmissionOf(assignmentId, student.id());
  if (existing) {
    // BR2: only before the effective deadline, and not once it is marked.
    if (late > 0) {
      throw RuleError("the deadline has passed, you cannot change your work");
    }
    if (marks_.findMark(existing->id)) {
      throw RuleError("your work is already marked");
    }
    existing->submittedAt = now;
    existing->content = content;
    submissions_.replaceSubmission(*existing);
    r.submissionId = existing->id;
    r.replaced = true;
    return r;
  }
  if (late > 0 && !confirmLate) {
    r.outcome = SubmitResult::Outcome::kNeedsConfirmation;
    return r;
  }
  Submission s;
  s.assignmentId = assignmentId;
  s.studentId = student.id();
  s.submittedAt = now;
  s.content = content;
  r.submissionId = submissions_.saveSubmission(s);
  return r;
}

std::vector<AssignmentView> SubmissionService::assignmentsFor(
    const User& student) {
  requireRole(student, Role::kStudent);
  std::vector<AssignmentView> out;
  for (const auto& a : assignments_.listForStudent(student.id())) {
    AssignmentView v;
    v.assignment = a;
    v.effectiveDeadline = assignments_.effectiveDeadline(a.id, student.id());
    v.status = "NOT SUBMITTED";
    if (auto s = submissions_.findSubmissionOf(a.id, student.id())) {
      v.status = s->submittedAt > v.effectiveDeadline ? "LATE" : "SUBMITTED";
      auto m = marks_.findMark(s->id);
      if (m && m->status == MarkStatus::kReleased) v.status = "MARK RELEASED";
    }
    out.push_back(v);
  }
  return out;
}

std::vector<MarkRow> SubmissionService::releasedMarks(const User& student) {
  requireRole(student, Role::kStudent);
  return marks_.releasedForStudent(student.id());
}

// --------------------------------------------------------------- marking
MarkData MarkingService::mark(const User& lecturer, int submissionId,
                              double rawMark, const std::string& feedback) {
  requireRole(lecturer, Role::kLecturer);
  auto sub = submissions_.findSubmission(submissionId);
  if (!sub) throw NotFoundError("submission not found");
  auto a = assignments_.findAssignment(sub->assignmentId);
  if (!a) throw NotFoundError("assignment not found");
  if (!courses_.isLecturerOf(lecturer.id(), a->courseId)) {
    throw PermissionError("you do not teach this course");
  }
  requireMarkInRange(rawMark, a->maxMark);
  std::string fb = trim(feedback);
  if (fb.empty() || fb.size() > kMaxText) {
    throw ValidationError("feedback must be 1 to 2000 characters");
  }
  Time now = clock_();
  Time deadline = assignments_.effectiveDeadline(a->id, sub->studentId);
  int late = daysLate(deadline, sub->submittedAt);
  double fin = finalMark(rawMark, penalty_.penalty(late, a->maxMark));

  Transaction tx(uow_);
  AuditEntry entry;
  entry.actorId = lecturer.id();
  entry.at = now;
  entry.submissionId = submissionId;
  MarkData result;
  if (auto existing = marks_.findMark(submissionId)) {
    Mark m(*existing);
    m.amend(rawMark, fin, fb, lecturer.id(), now);  // BR3 check is inside
    entry.action = AuditAction::kAmend;
    entry.oldValue = describe(*existing);
    entry.newValue = describe(m.data());
    entry.reason = fb;
    result = m.data();
  } else {
    result.submissionId = submissionId;
    result.rawMark = rawMark;
    result.daysLate = late;
    result.finalMark = fin;
    result.feedback = fb;
    result.status = MarkStatus::kMarked;
    result.markedBy = lecturer.id();
    result.markedAt = now;
    entry.action = AuditAction::kMark;
    entry.newValue = describe(result);
    entry.reason = fb;
  }
  result.daysLate = late;
  marks_.saveMark(result);
  audit_.append(entry);
  tx.commit();
  return result;
}

std::vector<Submission> MarkingService::unmarked(const User& lecturer) {
  requireRole(lecturer, Role::kLecturer);
  return submissions_.unmarkedForLecturer(lecturer.id());
}

std::vector<MarkRow> MarkingService::returned(const User& lecturer) {
  requireRole(lecturer, Role::kLecturer);
  return marks_.returnedForLecturer(lecturer.id());
}

// ------------------------------------------------------------ moderation
namespace {

// A checked downcast, so a wrong kind of user is refused instead of read as
// something it is not.
const HeadOfDepartment& asHead(const User& u) {
  const auto* head = dynamic_cast<const HeadOfDepartment*>(&u);
  if (head == nullptr) throw PermissionError("only a Head of Department can do this");
  return *head;
}

}  // namespace

ModerationService::Loaded ModerationService::loadFor(const User& hod,
                                                     int submissionId) {
  requireRole(hod, Role::kHod);
  const auto& head = asHead(hod);
  auto sub = submissions_.findSubmission(submissionId);
  if (!sub) throw NotFoundError("submission not found");
  auto a = assignments_.findAssignment(sub->assignmentId);
  auto data = marks_.findMark(submissionId);
  if (!a || !data) throw NotFoundError("there is no mark for this submission");
  auto dept = marks_.departmentOfSubmission(submissionId);
  if (head.deptId() == 0 || !dept || *dept != head.deptId()) {
    throw PermissionError("this course is not in your department");
  }
  if (data->markedBy == hod.id()) {  // assumption A4
    throw PermissionError("you cannot moderate your own marks");
  }
  return Loaded{Mark(*data), a->maxMark};
}

void ModerationService::record(const User& hod, int submissionId,
                               AuditAction action, const MarkData& before,
                               const MarkData& after,
                               const std::string& reason) {
  AuditEntry e;
  e.actorId = hod.id();
  e.at = clock_();
  e.action = action;
  e.submissionId = submissionId;
  e.oldValue = describe(before);
  e.newValue = describe(after);
  e.reason = reason;
  audit_.append(e);
}

std::vector<MarkRow> ModerationService::listPending(const User& hod) {
  requireRole(hod, Role::kHod);
  const auto& head = asHead(hod);
  if (head.deptId() == 0) return {};
  return marks_.pendingForDepartment(head.deptId());
}

void ModerationService::approve(const User& hod, int submissionId) {
  Loaded l = loadFor(hod, submissionId);
  MarkData before = l.mark.data();
  Transaction tx(uow_);
  l.mark.approve(hod.id());
  marks_.saveMark(l.mark.data());
  record(hod, submissionId, AuditAction::kApprove, before, l.mark.data(), "");
  tx.commit();
}

void ModerationService::adjust(const User& hod, int submissionId, double rawMark,
                               const std::string& reason) {
  Loaded l = loadFor(hod, submissionId);
  std::string why = trim(reason);
  if (why.empty() || why.size() > kMaxText) {
    throw ValidationError("an adjustment needs a reason");
  }
  requireMarkInRange(rawMark, l.maxMark);
  MarkData before = l.mark.data();
  double fin = finalMark(rawMark, penalty_.penalty(before.daysLate, l.maxMark));
  Transaction tx(uow_);
  l.mark.adjust(hod.id(), rawMark, fin);
  marks_.saveMark(l.mark.data());
  record(hod, submissionId, AuditAction::kAdjust, before, l.mark.data(), why);
  tx.commit();
}

void ModerationService::returnMark(const User& hod, int submissionId,
                                   const std::string& reason) {
  Loaded l = loadFor(hod, submissionId);
  std::string why = trim(reason);
  if (why.empty() || why.size() > kMaxText) {
    throw ValidationError("returning a mark needs a reason");
  }
  MarkData before = l.mark.data();
  Transaction tx(uow_);
  l.mark.returnForRemark(hod.id());
  marks_.saveMark(l.mark.data());
  record(hod, submissionId, AuditAction::kReturn, before, l.mark.data(), why);
  tx.commit();
}

void ModerationService::release(const User& hod, int submissionId) {
  Loaded l = loadFor(hod, submissionId);
  MarkData before = l.mark.data();
  Transaction tx(uow_);
  l.mark.release(clock_());
  marks_.saveMark(l.mark.data());
  record(hod, submissionId, AuditAction::kRelease, before, l.mark.data(), "");
  tx.commit();
}

}  // namespace lms
