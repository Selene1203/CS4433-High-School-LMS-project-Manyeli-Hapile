#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lms/penalty_strategy.hpp"
#include "lms/repositories.hpp"

namespace lms {

/// Throws PermissionError unless the actor has the given role (FR-02).
void requireRole(const User& actor, Role role);

// ---------------------------------------------------------------- login
class AuthService {
 public:
  AuthService(IUserRepository& users, Clock clock)
      : users_(users), clock_(std::move(clock)) {}

  /// FR-01. Locks the account after 5 wrong passwords in a row.
  std::unique_ptr<User> login(const std::string& username,
                              const std::string& password);
  /// FR-03.
  void changePassword(const User& user, const std::string& oldPassword,
                      const std::string& newPassword);

 private:
  IUserRepository& users_;
  Clock clock_;
};

// ---------------------------------------------------------------- admin
class AdminService {
 public:
  AdminService(IUserRepository& users, ICourseRepository& courses,
               IAuditRepository& audit, Clock clock)
      : users_(users), courses_(courses), audit_(audit),
        clock_(std::move(clock)) {}

  int createUser(const User& admin, Role role, const std::string& username,
                 const std::string& fullName, const std::string& password);
  void setActive(const User& admin, int userId, bool active);
  /// Sets a new password and clears the lock (FR-01, FR-04).
  void resetPassword(const User& admin, int userId,
                     const std::string& newPassword);
  /// Removes the personal details and keeps the marks (NFR-10).
  void anonymiseUser(const User& admin, int userId);
  std::vector<UserRecord> listUsers(const User& admin);

  int createDepartment(const User& admin, const std::string& name);
  void assignHod(const User& admin, int deptId, int userId);
  int createCourse(const User& admin, int deptId, const std::string& code,
                   const std::string& name);
  void assignLecturer(const User& admin, int courseId, int lecturerId);
  void enrol(const User& admin, int studentId, int courseId);

  std::vector<AuditEntry> auditLog(const User& admin);
  /// FR-21, one line per entry.
  void exportAudit(const User& admin, const std::string& path);

 private:
  IUserRepository& users_;
  ICourseRepository& courses_;
  IAuditRepository& audit_;
  Clock clock_;
};

// ----------------------------------------------------------- assignments
class AssignmentService {
 public:
  AssignmentService(IAssignmentRepository& assignments,
                    ICourseRepository& courses, IMarkRepository& marks,
                    ISubmissionRepository& submissions,
                    IAuditRepository& audit, IUnitOfWork& uow, Clock clock)
      : assignments_(assignments), courses_(courses), marks_(marks),
        submissions_(submissions), audit_(audit), uow_(uow),
        clock_(std::move(clock)) {}

  /// FR-07.
  int createAssignment(const User& lecturer, int courseId,
                       const std::string& title,
                       const std::string& description, Time deadline,
                       int maxMark);
  /// FR-08, BR4.
  void grantExtension(const User& lecturer, int assignmentId, int studentId,
                      Time newDeadline, const std::string& reason);
  /// CR1: the same extension for every enrolled student whose work is not
  /// marked yet and whose deadline would actually get later. Returns how many
  /// students got it. All or nothing, in one transaction.
  int extendForClass(const User& lecturer, int assignmentId, Time newDeadline,
                     const std::string& reason);
  std::vector<Assignment> listForCourse(const User& lecturer, int courseId);

 private:
  /// Saves one extension and its audit entry. The caller has done the checks
  /// and holds the transaction.
  void writeExtension(int lecturerId, int assignmentId, int studentId,
                      Time current, Time newDeadline, const std::string& reason);

  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  IMarkRepository& marks_;
  ISubmissionRepository& submissions_;
  IAuditRepository& audit_;
  IUnitOfWork& uow_;
  Clock clock_;
};

// ------------------------------------------------------------ submitting
struct SubmitResult {
  enum class Outcome { kSaved, kNeedsConfirmation };
  Outcome outcome = Outcome::kSaved;
  int submissionId = 0;
  bool late = false;
  int daysLate = 0;
  bool replaced = false;
  /// Marks that will come off at the maximum mark, shown in the warning.
  double penaltyOnMax = 0;
};

/// One line of the student's assignment list (FR-18).
struct AssignmentView {
  Assignment assignment;
  Time effectiveDeadline = 0;
  /// NOT SUBMITTED, SUBMITTED, LATE or MARK RELEASED. A mark that is not
  /// released yet is never shown (FR-17).
  std::string status;
};

class SubmissionService {
 public:
  SubmissionService(ISubmissionRepository& submissions,
                    IAssignmentRepository& assignments,
                    ICourseRepository& courses, IMarkRepository& marks,
                    const PenaltyStrategy& penalty, Clock clock)
      : submissions_(submissions), assignments_(assignments),
        courses_(courses), marks_(marks), penalty_(penalty),
        clock_(std::move(clock)) {}

  /// FR-09, FR-10, BR1, BR2.
  SubmitResult submit(const User& student, int assignmentId,
                      const std::string& content, bool confirmLate);
  std::vector<AssignmentView> assignmentsFor(const User& student);
  /// FR-17, released marks only.
  std::vector<MarkRow> releasedMarks(const User& student);

 private:
  ISubmissionRepository& submissions_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  IMarkRepository& marks_;
  const PenaltyStrategy& penalty_;
  Clock clock_;
};

// --------------------------------------------------------------- marking
class MarkingService {
 public:
  MarkingService(ISubmissionRepository& submissions,
                 IAssignmentRepository& assignments, ICourseRepository& courses,
                 IMarkRepository& marks, IAuditRepository& audit,
                 IUnitOfWork& uow, const PenaltyStrategy& penalty, Clock clock)
      : submissions_(submissions), assignments_(assignments),
        courses_(courses), marks_(marks), audit_(audit), uow_(uow),
        penalty_(penalty), clock_(std::move(clock)) {}

  /// FR-12 and FR-13, FR-11, FR-19.
  MarkData mark(const User& lecturer, int submissionId, double rawMark,
                const std::string& feedback);
  std::vector<Submission> unmarked(const User& lecturer);
  std::vector<MarkRow> returned(const User& lecturer);

 private:
  ISubmissionRepository& submissions_;
  IAssignmentRepository& assignments_;
  ICourseRepository& courses_;
  IMarkRepository& marks_;
  IAuditRepository& audit_;
  IUnitOfWork& uow_;
  const PenaltyStrategy& penalty_;
  Clock clock_;
};

// ------------------------------------------------------------ moderation
class ModerationService {
 public:
  ModerationService(ISubmissionRepository& submissions,
                    IAssignmentRepository& assignments, IMarkRepository& marks,
                    IAuditRepository& audit, IUnitOfWork& uow,
                    const PenaltyStrategy& penalty, Clock clock)
      : submissions_(submissions), assignments_(assignments), marks_(marks),
        audit_(audit), uow_(uow), penalty_(penalty), clock_(std::move(clock)) {}

  /// FR-14.
  std::vector<MarkRow> listPending(const User& hod);
  /// FR-15.
  void approve(const User& hod, int submissionId);
  void adjust(const User& hod, int submissionId, double rawMark,
              const std::string& reason);
  void returnMark(const User& hod, int submissionId, const std::string& reason);
  /// FR-16, BR3.
  void release(const User& hod, int submissionId);

 private:
  struct Loaded {
    Mark mark;
    int maxMark;
  };
  Loaded loadFor(const User& hod, int submissionId);
  void record(const User& hod, int submissionId, AuditAction action,
              const MarkData& before, const MarkData& after,
              const std::string& reason);

  ISubmissionRepository& submissions_;
  IAssignmentRepository& assignments_;
  IMarkRepository& marks_;
  IAuditRepository& audit_;
  IUnitOfWork& uow_;
  const PenaltyStrategy& penalty_;
  Clock clock_;
};

}  // namespace lms
