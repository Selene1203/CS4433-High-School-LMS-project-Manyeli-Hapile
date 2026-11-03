#pragma once

#include "lms/database.hpp"
#include "lms/repositories.hpp"

namespace lms {

/// Implements every repository interface on one SQLite connection, so a
/// service can group calls to several of them into one transaction.
class SqliteRepository final : public IUnitOfWork,
                               public IUserRepository,
                               public ICourseRepository,
                               public IAssignmentRepository,
                               public ISubmissionRepository,
                               public IMarkRepository,
                               public IAuditRepository {
 public:
  explicit SqliteRepository(Database& db) : db_(db) {}

  // IUnitOfWork
  void begin() override;
  void commit() override;
  void rollback() override;

  // IUserRepository
  int addUser(const UserRecord& user) override;
  void updateUser(const UserRecord& user) override;
  std::optional<UserRecord> findUser(int id) override;
  std::optional<UserRecord> findUserByName(const std::string& name) override;
  std::vector<UserRecord> listUsers() override;
  std::optional<int> departmentOfHod(int userId) override;

  // ICourseRepository
  int addDepartment(const std::string& name) override;
  void setHod(int deptId, int userId) override;
  bool departmentExists(int deptId) override;
  int addCourse(int deptId, const std::string& code,
                const std::string& name) override;
  std::optional<CourseInfo> findCourse(int courseId) override;
  std::vector<CourseInfo> listCourses() override;
  void addLecturer(int courseId, int lecturerId) override;
  void enrol(int studentId, int courseId, Time when) override;
  bool isLecturerOf(int lecturerId, int courseId) override;
  bool isEnrolled(int studentId, int courseId) override;
  std::vector<CourseInfo> coursesOfLecturer(int lecturerId) override;
  std::vector<CourseInfo> coursesOfStudent(int studentId) override;
  std::vector<int> studentsOfCourse(int courseId) override;

  // IAssignmentRepository
  int addAssignment(const Assignment& a) override;
  std::optional<Assignment> findAssignment(int assignmentId) override;
  std::vector<Assignment> listForCourse(int courseId) override;
  std::vector<Assignment> listForStudent(int studentId) override;
  Time effectiveDeadline(int assignmentId, int studentId) override;
  std::optional<Extension> findExtension(int assignmentId,
                                         int studentId) override;
  void saveExtension(const Extension& e) override;

  // ISubmissionRepository
  std::optional<Submission> findSubmissionOf(int assignmentId,
                                             int studentId) override;
  std::optional<Submission> findSubmission(int submissionId) override;
  int saveSubmission(const Submission& s) override;
  void replaceSubmission(const Submission& s) override;
  std::vector<Submission> unmarkedForLecturer(int lecturerId) override;

  // IMarkRepository
  std::optional<MarkData> findMark(int submissionId) override;
  void saveMark(const MarkData& m) override;
  std::vector<MarkRow> pendingForDepartment(int deptId) override;
  std::vector<MarkRow> releasedForStudent(int studentId) override;
  std::vector<MarkRow> returnedForLecturer(int lecturerId) override;
  std::optional<int> departmentOfSubmission(int submissionId) override;
  std::optional<int> courseOfSubmission(int submissionId) override;

  // IAuditRepository
  void append(const AuditEntry& e) override;
  std::vector<AuditEntry> listEntries() override;

 private:
  Database& db_;
};

}  // namespace lms
