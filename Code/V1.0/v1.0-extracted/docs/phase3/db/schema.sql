-- Moderated LMS, SQLite schema (BCNF)
PRAGMA foreign_keys = ON;

CREATE TABLE app_user (
    user_id        INTEGER PRIMARY KEY,
    username       TEXT    NOT NULL UNIQUE,
    password_hash  TEXT    NOT NULL,
    full_name      TEXT    NOT NULL,
    role           TEXT    NOT NULL CHECK (role IN ('ADMIN','LECTURER','HOD','STUDENT')),
    failed_logins  INTEGER NOT NULL DEFAULT 0 CHECK (failed_logins >= 0),
    active         INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))
);

CREATE TABLE department (
    dept_id  INTEGER PRIMARY KEY,
    name     TEXT    NOT NULL UNIQUE,
    hod_id   INTEGER UNIQUE REFERENCES app_user(user_id)
);

CREATE TABLE course (
    course_id  INTEGER PRIMARY KEY,
    dept_id    INTEGER NOT NULL REFERENCES department(dept_id),
    code       TEXT    NOT NULL UNIQUE,
    name       TEXT    NOT NULL
);

CREATE TABLE course_lecturer (
    course_id    INTEGER NOT NULL REFERENCES course(course_id),
    lecturer_id  INTEGER NOT NULL REFERENCES app_user(user_id),
    PRIMARY KEY (course_id, lecturer_id)
);

CREATE TABLE enrolment (
    student_id   INTEGER NOT NULL REFERENCES app_user(user_id),
    course_id    INTEGER NOT NULL REFERENCES course(course_id),
    enrolled_on  TEXT    NOT NULL,
    PRIMARY KEY (student_id, course_id)
);

CREATE TABLE assignment (
    assignment_id  INTEGER PRIMARY KEY,
    course_id      INTEGER NOT NULL REFERENCES course(course_id),
    created_by     INTEGER NOT NULL REFERENCES app_user(user_id),
    title          TEXT    NOT NULL,
    description    TEXT,
    deadline       TEXT    NOT NULL,
    max_mark       INTEGER NOT NULL CHECK (max_mark > 0),
    UNIQUE (course_id, title)
);

CREATE TABLE extension (
    assignment_id  INTEGER NOT NULL REFERENCES assignment(assignment_id),
    student_id     INTEGER NOT NULL REFERENCES app_user(user_id),
    new_deadline   TEXT    NOT NULL,
    reason         TEXT    NOT NULL CHECK (length(reason) > 0),
    granted_by     INTEGER NOT NULL REFERENCES app_user(user_id),
    granted_at     TEXT    NOT NULL,
    PRIMARY KEY (assignment_id, student_id)
);

CREATE TABLE submission (
    submission_id  INTEGER PRIMARY KEY,
    assignment_id  INTEGER NOT NULL REFERENCES assignment(assignment_id),
    student_id     INTEGER NOT NULL REFERENCES app_user(user_id),
    submitted_at   TEXT    NOT NULL,
    content        TEXT    NOT NULL CHECK (length(content) > 0),
    UNIQUE (assignment_id, student_id)
);

CREATE TABLE mark (
    submission_id  INTEGER PRIMARY KEY REFERENCES submission(submission_id),
    raw_mark       REAL    NOT NULL CHECK (raw_mark >= 0),
    days_late      INTEGER NOT NULL CHECK (days_late >= 0),
    final_mark     REAL    NOT NULL CHECK (final_mark >= 0),
    feedback       TEXT    NOT NULL,
    status         TEXT    NOT NULL CHECK (status IN ('MARKED','RETURNED','MODERATED','RELEASED')),
    marked_by      INTEGER NOT NULL REFERENCES app_user(user_id),
    marked_at      TEXT    NOT NULL,
    moderated_by   INTEGER REFERENCES app_user(user_id),
    released_at    TEXT,
    CHECK (final_mark <= raw_mark),
    CHECK (status <> 'RELEASED' OR released_at IS NOT NULL)
);

CREATE TABLE audit_log (
    audit_id       INTEGER PRIMARY KEY,
    actor_id       INTEGER NOT NULL REFERENCES app_user(user_id),
    at             TEXT    NOT NULL,
    action         TEXT    NOT NULL CHECK (action IN
                   ('MARK','AMEND','APPROVE','ADJUST','RETURN','RELEASE','EXTEND')),
    submission_id  INTEGER REFERENCES submission(submission_id),
    ext_assignment_id INTEGER,
    ext_student_id    INTEGER,
    old_value      TEXT,
    new_value      TEXT,
    reason         TEXT,
    FOREIGN KEY (ext_assignment_id, ext_student_id)
        REFERENCES extension(assignment_id, student_id),
    CHECK ((submission_id IS NOT NULL AND ext_assignment_id IS NULL AND ext_student_id IS NULL)
        OR (submission_id IS NULL AND ext_assignment_id IS NOT NULL AND ext_student_id IS NOT NULL))
);

CREATE INDEX idx_submission_student ON submission(student_id);
CREATE INDEX idx_mark_status        ON mark(status);
CREATE INDEX idx_enrolment_course   ON enrolment(course_id);
CREATE INDEX idx_assignment_course  ON assignment(course_id);

-- the audit log is append only (NFR-06)
CREATE TRIGGER audit_no_update BEFORE UPDATE ON audit_log
BEGIN SELECT RAISE(ABORT, 'audit entries cannot be changed'); END;

CREATE TRIGGER audit_no_delete BEFORE DELETE ON audit_log
BEGIN SELECT RAISE(ABORT, 'audit entries cannot be deleted'); END;
