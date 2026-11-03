#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lms/domain.hpp"
#include "lms/user.hpp"

namespace lms {

/// Lets a service group several writes into one transaction (NFR-06).
class IUnitOfWork {
 public:
  virtual ~IUnitOfWork() = default;
  virtual void begin() = 0;
  virtual void commit() = 0;
  virtual void rollback() = 0;
};

/// Rolls back unless commit() was called, so an exception undoes the lot.
class Transaction {
 public:
  explicit Transaction(IUnitOfWork& uow) : uow_(uow) { uow_.begin(); }
  ~Transaction() {
    if (!done_) {
      try {
        uow_.rollback();
      } catch (...) {
      }
    }
  }
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  void commit() {
    uow_.commit();
    done_ = true;
  }

 private:
  IUnitOfWork& uow_;
  bool done_ = false;
};

class IUserRepository {
 public:
  virtual ~IUserRepository() = default;
  virtual int addUser(const UserRecord& user) = 0;
  virtual void updateUser(const UserRecord& user) = 0;
  virtual std::optional<UserRecord> findUser(int id) = 0;
  virtual std::optional<UserRecord> findUserByName(const std::string& name) = 0;
  virtual std::vector<UserRecord> listUsers() = 0;
  /// Department the user heads, if any.
  virtual std::optional<int> departmentOfHod(int userId) = 0;
};

struct CourseInfo {
  int id = 0;
  int deptId = 0;
  std::string code;
  std::string name;
};

class ICourseRepository {
 public:
  virtual ~ICourseRepository() = default;
  virtual int addDepartment(const std::string& name) = 0;
  virtual void setHod(int deptId, int userId) = 0;
  virtual bool departmentExists(int deptId) = 0;
  virtual int addCourse(int deptId, const std::string& code,
                        const std::string& name) = 0;
  virtual std::optional<CourseInfo> findCourse(int courseId) = 0;
  virtual std::vector<CourseInfo> listCourses() = 0;
  virtual void addLecturer(int courseId, int lecturerId) = 0;
  virtual void enrol(int studentId, int courseId, Time when) = 0;
  virtual bool isLecturerOf(int lecturerId, int courseId) = 0;
  virtual bool isEnrolled(int studentId, int courseId) = 0;
  virtual std::vector<CourseInfo> coursesOfLecturer(int lecturerId) = 0;
  virtual std::vector<CourseInfo> coursesOfStudent(int studentId) = 0;
  /// User ids of every student enrolled in the course (CR1).
  virtual std::vector<int> studentsOfCourse(int courseId) = 0;
};

class IAssignmentRepository {
 public:
  virtual ~IAssignmentRepository() = default;
  virtual int addAssignment(const Assignment& a) = 0;
  virtual std::optional<Assignment> findAssignment(int assignmentId) = 0;
  virtual std::vector<Assignment> listForCourse(int courseId) = 0;
  /// Assignments of every course the student is enrolled in.
  virtual std::vector<Assignment> listForStudent(int studentId) = 0;
  /// The extension for this student if there is one, otherwise the
  /// assignment deadline (BR2).
  virtual Time effectiveDeadline(int assignmentId, int studentId) = 0;
  virtual std::optional<Extension> findExtension(int assignmentId,
                                                 int studentId) = 0;
  virtual void saveExtension(const Extension& e) = 0;
};

class ISubmissionRepository {
 public:
  virtual ~ISubmissionRepository() = default;
  virtual std::optional<Submission> findSubmissionOf(int assignmentId, int studentId) = 0;
  virtual std::optional<Submission> findSubmission(int submissionId) = 0;
  virtual int saveSubmission(const Submission& s) = 0;
  virtual void replaceSubmission(const Submission& s) = 0;
  /// Submissions of the lecturer's courses that have no mark yet.
  virtual std::vector<Submission> unmarkedForLecturer(int lecturerId) = 0;
};

/// A mark with the bits the menus need to show it.
struct MarkRow {
  MarkData mark;
  int assignmentId = 0;
  int studentId = 0;
  int courseId = 0;
  std::string assignmentTitle;
  int maxMark = 0;
  std::string studentName;
};

class IMarkRepository {
 public:
  virtual ~IMarkRepository() = default;
  virtual std::optional<MarkData> findMark(int submissionId) = 0;
  virtual void saveMark(const MarkData& m) = 0;
  /// Marked or moderated, not released, for one department (FR-14).
  virtual std::vector<MarkRow> pendingForDepartment(int deptId) = 0;
  /// Released marks only (FR-17).
  virtual std::vector<MarkRow> releasedForStudent(int studentId) = 0;
  /// Marks sent back to this lecturer for re-marking (FR-13).
  virtual std::vector<MarkRow> returnedForLecturer(int lecturerId) = 0;
  virtual std::optional<int> departmentOfSubmission(int submissionId) = 0;
  virtual std::optional<int> courseOfSubmission(int submissionId) = 0;
};

enum class AuditAction { kMark, kAmend, kApprove, kAdjust, kReturn, kRelease,
                         kExtend };
std::string toString(AuditAction a);

struct AuditEntry {
  int id = 0;
  int actorId = 0;
  Time at = 0;
  AuditAction action = AuditAction::kMark;
  /// Set for mark changes. For EXTEND both extension fields are set instead.
  std::optional<int> submissionId;
  std::optional<int> extAssignmentId;
  std::optional<int> extStudentId;
  std::string oldValue;
  std::string newValue;
  std::string reason;
};

class IAuditRepository {
 public:
  virtual ~IAuditRepository() = default;
  virtual void append(const AuditEntry& e) = 0;
  virtual std::vector<AuditEntry> listEntries() = 0;
};

}  // namespace lms
