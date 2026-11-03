#include "lms/sqlite_repository.hpp"

#include "lms/error.hpp"

namespace lms {

std::string toString(AuditAction a) {
  switch (a) {
    case AuditAction::kMark: return "MARK";
    case AuditAction::kAmend: return "AMEND";
    case AuditAction::kApprove: return "APPROVE";
    case AuditAction::kAdjust: return "ADJUST";
    case AuditAction::kReturn: return "RETURN";
    case AuditAction::kRelease: return "RELEASE";
    case AuditAction::kExtend: return "EXTEND";
  }
  return "";
}

namespace {

int toInt(std::int64_t v) { return static_cast<int>(v); }

UserRecord userFrom(const Statement& s) {
  UserRecord u;
  u.id = toInt(s.getInt(0));
  u.username = s.getText(1);
  u.passwordHash = s.getText(2);
  u.fullName = s.getText(3);
  u.role = parseRole(s.getText(4));
  u.failedLogins = toInt(s.getInt(5));
  u.active = s.getInt(6) != 0;
  return u;
}

const char* kUserCols =
    "user_id, username, password_hash, full_name, role, failed_logins, active";

CourseInfo courseFrom(const Statement& s) {
  CourseInfo c;
  c.id = toInt(s.getInt(0));
  c.deptId = toInt(s.getInt(1));
  c.code = s.getText(2);
  c.name = s.getText(3);
  return c;
}

Assignment assignmentFrom(const Statement& s) {
  Assignment a;
  a.id = toInt(s.getInt(0));
  a.courseId = toInt(s.getInt(1));
  a.createdBy = toInt(s.getInt(2));
  a.title = s.getText(3);
  a.description = s.getText(4);
  a.deadline = parseTime(s.getText(5));
  a.maxMark = toInt(s.getInt(6));
  return a;
}

const char* kAssignmentCols =
    "assignment_id, course_id, created_by, title, ifnull(description,''), "
    "deadline, max_mark";

Submission submissionFrom(const Statement& s) {
  Submission x;
  x.id = toInt(s.getInt(0));
  x.assignmentId = toInt(s.getInt(1));
  x.studentId = toInt(s.getInt(2));
  x.submittedAt = parseTime(s.getText(3));
  x.content = s.getText(4);
  return x;
}

const char* kSubmissionCols =
    "submission_id, assignment_id, student_id, submitted_at, content";

MarkData markFrom(const Statement& s, int base = 0) {
  MarkData m;
  m.submissionId = toInt(s.getInt(base + 0));
  m.rawMark = s.getReal(base + 1);
  m.daysLate = toInt(s.getInt(base + 2));
  m.finalMark = s.getReal(base + 3);
  m.feedback = s.getText(base + 4);
  m.status = parseMarkStatus(s.getText(base + 5));
  m.markedBy = toInt(s.getInt(base + 6));
  m.markedAt = parseTime(s.getText(base + 7));
  if (!s.isNull(base + 8)) m.moderatedBy = toInt(s.getInt(base + 8));
  if (!s.isNull(base + 9)) m.releasedAt = parseTime(s.getText(base + 9));
  return m;
}

const char* kMarkCols =
    "m.submission_id, m.raw_mark, m.days_late, m.final_mark, m.feedback, "
    "m.status, m.marked_by, m.marked_at, m.moderated_by, m.released_at";

// Marks joined to submission, assignment, course and student for the menus.
const char* kMarkRowSql =
    "SELECT m.submission_id, m.raw_mark, m.days_late, m.final_mark, "
    "m.feedback, m.status, m.marked_by, m.marked_at, m.moderated_by, "
    "m.released_at, a.assignment_id, s.student_id, a.course_id, a.title, "
    "a.max_mark, u.full_name "
    "FROM mark m JOIN submission s ON s.submission_id = m.submission_id "
    "JOIN assignment a ON a.assignment_id = s.assignment_id "
    "JOIN course c ON c.course_id = a.course_id "
    "JOIN app_user u ON u.user_id = s.student_id ";

MarkRow markRowFrom(const Statement& s) {
  MarkRow r;
  r.mark = markFrom(s, 0);
  r.assignmentId = toInt(s.getInt(10));
  r.studentId = toInt(s.getInt(11));
  r.courseId = toInt(s.getInt(12));
  r.assignmentTitle = s.getText(13);
  r.maxMark = toInt(s.getInt(14));
  r.studentName = s.getText(15);
  return r;
}

}  // namespace

// ---------------- unit of work ----------------
void SqliteRepository::begin() { db_.exec("BEGIN IMMEDIATE"); }
void SqliteRepository::commit() { db_.exec("COMMIT"); }
void SqliteRepository::rollback() { db_.exec("ROLLBACK"); }

// ---------------- users ----------------
int SqliteRepository::addUser(const UserRecord& u) {
  Statement st(db_,
               "INSERT INTO app_user(username, password_hash, full_name, role,"
               " failed_logins, active) VALUES (?,?,?,?,?,?)");
  st.bindText(1, u.username).bindText(2, u.passwordHash).bindText(3, u.fullName);
  st.bindText(4, toString(u.role)).bindInt(5, u.failedLogins);
  st.bindInt(6, u.active ? 1 : 0);
  st.run();
  return toInt(db_.lastInsertId());
}

void SqliteRepository::updateUser(const UserRecord& u) {
  Statement st(db_,
               "UPDATE app_user SET username=?, password_hash=?, full_name=?,"
               " role=?, failed_logins=?, active=? WHERE user_id=?");
  st.bindText(1, u.username).bindText(2, u.passwordHash).bindText(3, u.fullName);
  st.bindText(4, toString(u.role)).bindInt(5, u.failedLogins);
  st.bindInt(6, u.active ? 1 : 0).bindInt(7, u.id);
  st.run();
}

std::optional<UserRecord> SqliteRepository::findUser(int id) {
  Statement st(db_, std::string("SELECT ") + kUserCols +
                        " FROM app_user WHERE user_id=?");
  st.bindInt(1, id);
  if (!st.step()) return std::nullopt;
  return userFrom(st);
}

std::optional<UserRecord> SqliteRepository::findUserByName(
    const std::string& name) {
  Statement st(db_, std::string("SELECT ") + kUserCols +
                        " FROM app_user WHERE username=?");
  st.bindText(1, name);
  if (!st.step()) return std::nullopt;
  return userFrom(st);
}

std::vector<UserRecord> SqliteRepository::listUsers() {
  Statement st(db_, std::string("SELECT ") + kUserCols +
                        " FROM app_user ORDER BY user_id");
  std::vector<UserRecord> out;
  while (st.step()) out.push_back(userFrom(st));
  return out;
}

std::optional<int> SqliteRepository::departmentOfHod(int userId) {
  Statement st(db_, "SELECT dept_id FROM department WHERE hod_id=?");
  st.bindInt(1, userId);
  if (!st.step()) return std::nullopt;
  return toInt(st.getInt(0));
}

// ---------------- departments and courses ----------------
int SqliteRepository::addDepartment(const std::string& name) {
  Statement st(db_, "INSERT INTO department(name) VALUES (?)");
  st.bindText(1, name).run();
  return toInt(db_.lastInsertId());
}

void SqliteRepository::setHod(int deptId, int userId) {
  Statement st(db_, "UPDATE department SET hod_id=? WHERE dept_id=?");
  st.bindInt(1, userId).bindInt(2, deptId).run();
}

bool SqliteRepository::departmentExists(int deptId) {
  Statement st(db_, "SELECT 1 FROM department WHERE dept_id=?");
  st.bindInt(1, deptId);
  return st.step();
}

int SqliteRepository::addCourse(int deptId, const std::string& code,
                                const std::string& name) {
  Statement st(db_, "INSERT INTO course(dept_id, code, name) VALUES (?,?,?)");
  st.bindInt(1, deptId).bindText(2, code).bindText(3, name).run();
  return toInt(db_.lastInsertId());
}

std::optional<CourseInfo> SqliteRepository::findCourse(int courseId) {
  Statement st(db_,
               "SELECT course_id, dept_id, code, name FROM course WHERE "
               "course_id=?");
  st.bindInt(1, courseId);
  if (!st.step()) return std::nullopt;
  return courseFrom(st);
}

std::vector<CourseInfo> SqliteRepository::listCourses() {
  Statement st(db_,
               "SELECT course_id, dept_id, code, name FROM course ORDER BY "
               "code");
  std::vector<CourseInfo> out;
  while (st.step()) out.push_back(courseFrom(st));
  return out;
}

void SqliteRepository::addLecturer(int courseId, int lecturerId) {
  Statement st(db_,
               "INSERT INTO course_lecturer(course_id, lecturer_id) VALUES "
               "(?,?)");
  st.bindInt(1, courseId).bindInt(2, lecturerId).run();
}

void SqliteRepository::enrol(int studentId, int courseId, Time when) {
  Statement st(db_,
               "INSERT INTO enrolment(student_id, course_id, enrolled_on) "
               "VALUES (?,?,?)");
  st.bindInt(1, studentId).bindInt(2, courseId).bindText(3, formatTime(when));
  st.run();
}

bool SqliteRepository::isLecturerOf(int lecturerId, int courseId) {
  Statement st(db_,
               "SELECT 1 FROM course_lecturer WHERE lecturer_id=? AND "
               "course_id=?");
  st.bindInt(1, lecturerId).bindInt(2, courseId);
  return st.step();
}

bool SqliteRepository::isEnrolled(int studentId, int courseId) {
  Statement st(db_,
               "SELECT 1 FROM enrolment WHERE student_id=? AND course_id=?");
  st.bindInt(1, studentId).bindInt(2, courseId);
  return st.step();
}

std::vector<CourseInfo> SqliteRepository::coursesOfLecturer(int lecturerId) {
  Statement st(db_,
               "SELECT c.course_id, c.dept_id, c.code, c.name FROM course c "
               "JOIN course_lecturer l ON l.course_id=c.course_id WHERE "
               "l.lecturer_id=? ORDER BY c.code");
  st.bindInt(1, lecturerId);
  std::vector<CourseInfo> out;
  while (st.step()) out.push_back(courseFrom(st));
  return out;
}

std::vector<CourseInfo> SqliteRepository::coursesOfStudent(int studentId) {
  Statement st(db_,
               "SELECT c.course_id, c.dept_id, c.code, c.name FROM course c "
               "JOIN enrolment e ON e.course_id=c.course_id WHERE "
               "e.student_id=? ORDER BY c.code");
  st.bindInt(1, studentId);
  std::vector<CourseInfo> out;
  while (st.step()) out.push_back(courseFrom(st));
  return out;
}

std::vector<int> SqliteRepository::studentsOfCourse(int courseId) {
  Statement st(db_,
               "SELECT student_id FROM enrolment WHERE course_id=? ORDER BY "
               "student_id");
  st.bindInt(1, courseId);
  std::vector<int> out;
  while (st.step()) out.push_back(toInt(st.getInt(0)));
  return out;
}

// ---------------- assignments and extensions ----------------
int SqliteRepository::addAssignment(const Assignment& a) {
  Statement st(db_,
               "INSERT INTO assignment(course_id, created_by, title, "
               "description, deadline, max_mark) VALUES (?,?,?,?,?,?)");
  st.bindInt(1, a.courseId).bindInt(2, a.createdBy).bindText(3, a.title);
  st.bindText(4, a.description).bindText(5, formatTime(a.deadline));
  st.bindInt(6, a.maxMark).run();
  return toInt(db_.lastInsertId());
}

std::optional<Assignment> SqliteRepository::findAssignment(int id) {
  Statement st(db_, std::string("SELECT ") + kAssignmentCols +
                        " FROM assignment WHERE assignment_id=?");
  st.bindInt(1, id);
  if (!st.step()) return std::nullopt;
  return assignmentFrom(st);
}

std::vector<Assignment> SqliteRepository::listForCourse(int courseId) {
  Statement st(db_, std::string("SELECT ") + kAssignmentCols +
                        " FROM assignment WHERE course_id=? ORDER BY deadline");
  st.bindInt(1, courseId);
  std::vector<Assignment> out;
  while (st.step()) out.push_back(assignmentFrom(st));
  return out;
}

std::vector<Assignment> SqliteRepository::listForStudent(int studentId) {
  Statement st(db_,
               "SELECT a.assignment_id, a.course_id, a.created_by, a.title, "
               "ifnull(a.description,''), a.deadline, a.max_mark FROM "
               "assignment a JOIN enrolment e ON e.course_id=a.course_id "
               "WHERE e.student_id=? ORDER BY a.deadline");
  st.bindInt(1, studentId);
  std::vector<Assignment> out;
  while (st.step()) out.push_back(assignmentFrom(st));
  return out;
}

std::optional<Extension> SqliteRepository::findExtension(int assignmentId,
                                                         int studentId) {
  Statement st(db_,
               "SELECT assignment_id, student_id, new_deadline, reason, "
               "granted_by, granted_at FROM extension WHERE assignment_id=? "
               "AND student_id=?");
  st.bindInt(1, assignmentId).bindInt(2, studentId);
  if (!st.step()) return std::nullopt;
  Extension e;
  e.assignmentId = toInt(st.getInt(0));
  e.studentId = toInt(st.getInt(1));
  e.newDeadline = parseTime(st.getText(2));
  e.reason = st.getText(3);
  e.grantedBy = toInt(st.getInt(4));
  e.grantedAt = parseTime(st.getText(5));
  return e;
}

Time SqliteRepository::effectiveDeadline(int assignmentId, int studentId) {
  if (auto e = findExtension(assignmentId, studentId)) return e->newDeadline;
  auto a = findAssignment(assignmentId);
  if (!a) throw NotFoundError("assignment not found");
  return a->deadline;
}

void SqliteRepository::saveExtension(const Extension& e) {
  Statement st(db_,
               "INSERT OR REPLACE INTO extension(assignment_id, student_id, "
               "new_deadline, reason, granted_by, granted_at) VALUES "
               "(?,?,?,?,?,?)");
  st.bindInt(1, e.assignmentId).bindInt(2, e.studentId);
  st.bindText(3, formatTime(e.newDeadline)).bindText(4, e.reason);
  st.bindInt(5, e.grantedBy).bindText(6, formatTime(e.grantedAt));
  st.run();
}

// ---------------- submissions ----------------
std::optional<Submission> SqliteRepository::findSubmissionOf(int assignmentId,
                                                             int studentId) {
  Statement st(db_, std::string("SELECT ") + kSubmissionCols +
                        " FROM submission WHERE assignment_id=? AND "
                        "student_id=?");
  st.bindInt(1, assignmentId).bindInt(2, studentId);
  if (!st.step()) return std::nullopt;
  return submissionFrom(st);
}

std::optional<Submission> SqliteRepository::findSubmission(int submissionId) {
  Statement st(db_, std::string("SELECT ") + kSubmissionCols +
                        " FROM submission WHERE submission_id=?");
  st.bindInt(1, submissionId);
  if (!st.step()) return std::nullopt;
  return submissionFrom(st);
}

int SqliteRepository::saveSubmission(const Submission& s) {
  Statement st(db_,
               "INSERT INTO submission(assignment_id, student_id, "
               "submitted_at, content) VALUES (?,?,?,?)");
  st.bindInt(1, s.assignmentId).bindInt(2, s.studentId);
  st.bindText(3, formatTime(s.submittedAt)).bindText(4, s.content).run();
  return toInt(db_.lastInsertId());
}

void SqliteRepository::replaceSubmission(const Submission& s) {
  Statement st(db_,
               "UPDATE submission SET submitted_at=?, content=? WHERE "
               "submission_id=?");
  st.bindText(1, formatTime(s.submittedAt)).bindText(2, s.content);
  st.bindInt(3, s.id).run();
}

std::vector<Submission> SqliteRepository::unmarkedForLecturer(int lecturerId) {
  Statement st(db_,
               "SELECT s.submission_id, s.assignment_id, s.student_id, "
               "s.submitted_at, s.content FROM submission s JOIN assignment a "
               "ON a.assignment_id=s.assignment_id JOIN course_lecturer l ON "
               "l.course_id=a.course_id WHERE l.lecturer_id=? AND NOT EXISTS "
               "(SELECT 1 FROM mark m WHERE m.submission_id=s.submission_id) "
               "ORDER BY s.submission_id");
  st.bindInt(1, lecturerId);
  std::vector<Submission> out;
  while (st.step()) out.push_back(submissionFrom(st));
  return out;
}

// ---------------- marks ----------------
std::optional<MarkData> SqliteRepository::findMark(int submissionId) {
  Statement st(db_, std::string("SELECT ") + kMarkCols +
                        " FROM mark m WHERE m.submission_id=?");
  st.bindInt(1, submissionId);
  if (!st.step()) return std::nullopt;
  return markFrom(st);
}

void SqliteRepository::saveMark(const MarkData& m) {
  Statement st(db_,
               "INSERT INTO mark(submission_id, raw_mark, days_late, "
               "final_mark, feedback, status, marked_by, marked_at, "
               "moderated_by, released_at) VALUES (?,?,?,?,?,?,?,?,?,?) "
               "ON CONFLICT(submission_id) DO UPDATE SET raw_mark=excluded."
               "raw_mark, days_late=excluded.days_late, final_mark=excluded."
               "final_mark, feedback=excluded.feedback, status=excluded."
               "status, marked_by=excluded.marked_by, marked_at=excluded."
               "marked_at, moderated_by=excluded.moderated_by, released_at="
               "excluded.released_at");
  st.bindInt(1, m.submissionId).bindReal(2, m.rawMark).bindInt(3, m.daysLate);
  st.bindReal(4, m.finalMark).bindText(5, m.feedback);
  st.bindText(6, toString(m.status)).bindInt(7, m.markedBy);
  st.bindText(8, formatTime(m.markedAt));
  if (m.moderatedBy) st.bindInt(9, *m.moderatedBy); else st.bindNull(9);
  if (m.releasedAt) st.bindText(10, formatTime(*m.releasedAt));
  else st.bindNull(10);
  st.run();
}

std::vector<MarkRow> SqliteRepository::pendingForDepartment(int deptId) {
  Statement st(db_, std::string(kMarkRowSql) +
                        "WHERE c.dept_id=? AND m.status IN "
                        "('MARKED','MODERATED') ORDER BY m.submission_id");
  st.bindInt(1, deptId);
  std::vector<MarkRow> out;
  while (st.step()) out.push_back(markRowFrom(st));
  return out;
}

std::vector<MarkRow> SqliteRepository::releasedForStudent(int studentId) {
  Statement st(db_, std::string(kMarkRowSql) +
                        "WHERE s.student_id=? AND m.status='RELEASED' ORDER BY "
                        "m.submission_id");
  st.bindInt(1, studentId);
  std::vector<MarkRow> out;
  while (st.step()) out.push_back(markRowFrom(st));
  return out;
}

std::vector<MarkRow> SqliteRepository::returnedForLecturer(int lecturerId) {
  Statement st(db_, std::string(kMarkRowSql) +
                        "JOIN course_lecturer l ON l.course_id=a.course_id "
                        "WHERE l.lecturer_id=? AND m.status='RETURNED' ORDER "
                        "BY m.submission_id");
  st.bindInt(1, lecturerId);
  std::vector<MarkRow> out;
  while (st.step()) out.push_back(markRowFrom(st));
  return out;
}

std::optional<int> SqliteRepository::departmentOfSubmission(int submissionId) {
  Statement st(db_,
               "SELECT c.dept_id FROM submission s JOIN assignment a ON "
               "a.assignment_id=s.assignment_id JOIN course c ON "
               "c.course_id=a.course_id WHERE s.submission_id=?");
  st.bindInt(1, submissionId);
  if (!st.step()) return std::nullopt;
  return toInt(st.getInt(0));
}

std::optional<int> SqliteRepository::courseOfSubmission(int submissionId) {
  Statement st(db_,
               "SELECT a.course_id FROM submission s JOIN assignment a ON "
               "a.assignment_id=s.assignment_id WHERE s.submission_id=?");
  st.bindInt(1, submissionId);
  if (!st.step()) return std::nullopt;
  return toInt(st.getInt(0));
}

// ---------------- audit ----------------
void SqliteRepository::append(const AuditEntry& e) {
  Statement st(db_,
               "INSERT INTO audit_log(actor_id, at, action, submission_id, "
               "ext_assignment_id, ext_student_id, old_value, new_value, "
               "reason) VALUES (?,?,?,?,?,?,?,?,?)");
  st.bindInt(1, e.actorId).bindText(2, formatTime(e.at));
  st.bindText(3, toString(e.action));
  if (e.submissionId) st.bindInt(4, *e.submissionId); else st.bindNull(4);
  if (e.extAssignmentId) st.bindInt(5, *e.extAssignmentId); else st.bindNull(5);
  if (e.extStudentId) st.bindInt(6, *e.extStudentId); else st.bindNull(6);
  st.bindText(7, e.oldValue).bindText(8, e.newValue).bindText(9, e.reason);
  st.run();
}

std::vector<AuditEntry> SqliteRepository::listEntries() {
  Statement st(db_,
               "SELECT audit_id, actor_id, at, action, submission_id, "
               "ext_assignment_id, ext_student_id, ifnull(old_value,''), "
               "ifnull(new_value,''), ifnull(reason,'') FROM audit_log ORDER "
               "BY audit_id");
  std::vector<AuditEntry> out;
  while (st.step()) {
    AuditEntry e;
    e.id = toInt(st.getInt(0));
    e.actorId = toInt(st.getInt(1));
    e.at = parseTime(st.getText(2));
    std::string a = st.getText(3);
    for (AuditAction x : {AuditAction::kMark, AuditAction::kAmend,
                          AuditAction::kApprove, AuditAction::kAdjust,
                          AuditAction::kReturn, AuditAction::kRelease,
                          AuditAction::kExtend}) {
      if (toString(x) == a) e.action = x;
    }
    if (!st.isNull(4)) e.submissionId = toInt(st.getInt(4));
    if (!st.isNull(5)) e.extAssignmentId = toInt(st.getInt(5));
    if (!st.isNull(6)) e.extStudentId = toInt(st.getInt(6));
    e.oldValue = st.getText(7);
    e.newValue = st.getText(8);
    e.reason = st.getText(9);
    out.push_back(e);
  }
  return out;
}

}  // namespace lms
