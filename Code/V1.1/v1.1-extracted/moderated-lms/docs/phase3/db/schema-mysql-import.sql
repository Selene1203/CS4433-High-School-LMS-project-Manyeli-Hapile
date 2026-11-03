-- generated from erd.dbml, for tools that import MySQL SQL
CREATE TABLE `app_user` (
  `user_id` integer PRIMARY KEY AUTO_INCREMENT,
  `username` varchar(40) UNIQUE NOT NULL,
  `password_hash` varchar(128) NOT NULL,
  `full_name` varchar(80) NOT NULL,
  `role` varchar(10) NOT NULL COMMENT 'ADMIN, LECTURER, HOD or STUDENT',
  `failed_logins` integer NOT NULL DEFAULT 0,
  `active` boolean NOT NULL DEFAULT true
);

CREATE TABLE `department` (
  `dept_id` integer PRIMARY KEY AUTO_INCREMENT,
  `name` varchar(60) UNIQUE NOT NULL,
  `hod_id` integer UNIQUE COMMENT 'the Head of Department, if one is set'
);

CREATE TABLE `course` (
  `course_id` integer PRIMARY KEY AUTO_INCREMENT,
  `dept_id` integer NOT NULL,
  `code` varchar(12) UNIQUE NOT NULL,
  `name` varchar(80) NOT NULL
);

CREATE TABLE `course_lecturer` (
  `course_id` integer NOT NULL,
  `lecturer_id` integer NOT NULL,
  PRIMARY KEY (`course_id`, `lecturer_id`)
);

CREATE TABLE `enrolment` (
  `student_id` integer NOT NULL,
  `course_id` integer NOT NULL,
  `enrolled_on` date NOT NULL,
  PRIMARY KEY (`student_id`, `course_id`)
);

CREATE TABLE `assignment` (
  `assignment_id` integer PRIMARY KEY AUTO_INCREMENT,
  `course_id` integer NOT NULL,
  `created_by` integer NOT NULL,
  `title` varchar(120) NOT NULL,
  `description` text,
  `deadline` datetime NOT NULL,
  `max_mark` integer NOT NULL COMMENT 'must be above 0'
);

CREATE TABLE `extension` (
  `assignment_id` integer NOT NULL,
  `student_id` integer NOT NULL,
  `new_deadline` datetime NOT NULL,
  `reason` varchar(200) NOT NULL,
  `granted_by` integer NOT NULL,
  `granted_at` datetime NOT NULL,
  PRIMARY KEY (`assignment_id`, `student_id`)
);

CREATE TABLE `submission` (
  `submission_id` integer PRIMARY KEY AUTO_INCREMENT,
  `assignment_id` integer NOT NULL,
  `student_id` integer NOT NULL,
  `submitted_at` datetime NOT NULL,
  `content` text NOT NULL
);

CREATE TABLE `mark` (
  `submission_id` integer PRIMARY KEY,
  `raw_mark` decimal(6,2) NOT NULL,
  `days_late` integer NOT NULL,
  `final_mark` decimal(6,2) NOT NULL,
  `feedback` text NOT NULL,
  `status` varchar(10) NOT NULL COMMENT 'MARKED, RETURNED, MODERATED or RELEASED',
  `marked_by` integer NOT NULL,
  `marked_at` datetime NOT NULL,
  `moderated_by` integer,
  `released_at` datetime
);

CREATE TABLE `audit_log` (
  `audit_id` integer PRIMARY KEY AUTO_INCREMENT,
  `actor_id` integer NOT NULL,
  `at` datetime NOT NULL,
  `action` varchar(10) NOT NULL,
  `submission_id` integer,
  `ext_assignment_id` integer,
  `ext_student_id` integer,
  `old_value` varchar(80),
  `new_value` varchar(80),
  `reason` varchar(200)
);

CREATE UNIQUE INDEX `assignment_index_0` ON `assignment` (`course_id`, `title`);

CREATE UNIQUE INDEX `submission_index_1` ON `submission` (`assignment_id`, `student_id`);

ALTER TABLE `department` ADD FOREIGN KEY (`hod_id`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `course` ADD FOREIGN KEY (`dept_id`) REFERENCES `department` (`dept_id`);

ALTER TABLE `course_lecturer` ADD FOREIGN KEY (`course_id`) REFERENCES `course` (`course_id`);

ALTER TABLE `course_lecturer` ADD FOREIGN KEY (`lecturer_id`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `enrolment` ADD FOREIGN KEY (`student_id`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `enrolment` ADD FOREIGN KEY (`course_id`) REFERENCES `course` (`course_id`);

ALTER TABLE `assignment` ADD FOREIGN KEY (`course_id`) REFERENCES `course` (`course_id`);

ALTER TABLE `assignment` ADD FOREIGN KEY (`created_by`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `extension` ADD FOREIGN KEY (`assignment_id`) REFERENCES `assignment` (`assignment_id`);

ALTER TABLE `extension` ADD FOREIGN KEY (`student_id`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `extension` ADD FOREIGN KEY (`granted_by`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `submission` ADD FOREIGN KEY (`assignment_id`) REFERENCES `assignment` (`assignment_id`);

ALTER TABLE `submission` ADD FOREIGN KEY (`student_id`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `mark` ADD FOREIGN KEY (`submission_id`) REFERENCES `submission` (`submission_id`);

ALTER TABLE `mark` ADD FOREIGN KEY (`marked_by`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `mark` ADD FOREIGN KEY (`moderated_by`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `audit_log` ADD FOREIGN KEY (`actor_id`) REFERENCES `app_user` (`user_id`);

ALTER TABLE `audit_log` ADD FOREIGN KEY (`submission_id`) REFERENCES `submission` (`submission_id`);

ALTER TABLE `audit_log` ADD FOREIGN KEY (`ext_assignment_id`, `ext_student_id`) REFERENCES `extension` (`assignment_id`, `student_id`);
