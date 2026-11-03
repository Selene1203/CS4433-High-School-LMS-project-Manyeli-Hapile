#include "lms/console_ui.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

#include "lms/error.hpp"

namespace lms {

bool ConsoleUi::readLine(const std::string& prompt, std::string& line) {
  out_ << prompt << std::flush;
  return static_cast<bool>(std::getline(in_, line));
}

std::string ConsoleUi::ask(const std::string& prompt) {
  std::string line;
  if (!readLine(prompt, line)) throw Error("input ended");
  return line;
}

int ConsoleUi::askInt(const std::string& prompt) {
  std::string t = ask(prompt);
  char* end = nullptr;
  long v = std::strtol(t.c_str(), &end, 10);
  if (t.empty() || *end != '\0' || v < -2000000000L || v > 2000000000L) {
    throw ValidationError("please type a whole number");
  }
  return static_cast<int>(v);
}

double ConsoleUi::askNumber(const std::string& prompt) {
  std::string t = ask(prompt);
  char* end = nullptr;
  double v = std::strtod(t.c_str(), &end);
  if (t.empty() || *end != '\0') throw ValidationError("please type a number");
  return v;
}

Time ConsoleUi::askTime(const std::string& prompt) {
  return parseTime(ask(prompt + " (YYYY-MM-DD HH:MM): "));
}

std::string ConsoleUi::askWork() {
  out_ << "Type your work. Finish with a line that only says END.\n";
  std::string all, line;
  while (std::getline(in_, line)) {
    if (line == "END") return all;
    all += line + "\n";
  }
  throw Error("input ended");
}

int ConsoleUi::choose(const std::string& title,
                      const std::vector<std::string>& items) {
  out_ << "\n== " << title << " ==\n";
  for (std::size_t i = 0; i < items.size(); ++i) {
    out_ << "  " << (i + 1) << ". " << items[i] << "\n";
  }
  out_ << "  0. Log out\n";
  while (true) {
    std::string line;
    if (!readLine("Choice: ", line)) return -1;
    char* end = nullptr;
    long v = std::strtol(line.c_str(), &end, 10);
    if (!line.empty() && *end == '\0' && v >= 0 &&
        v <= static_cast<long>(items.size())) {
      return static_cast<int>(v);
    }
    out_ << "Please choose a number from the list.\n";
  }
}

void ConsoleUi::changePassword(const User& user) {
  std::string oldPw = ask("Old password: ");
  std::string newPw = ask("New password (at least 8 characters): ");
  s_.auth.changePassword(user, oldPw, newPw);
  out_ << "Password changed.\n";
}

void ConsoleUi::printMarkRows(const std::vector<MarkRow>& rows) {
  if (rows.empty()) out_ << "Nothing to show.\n";
  for (const auto& r : rows) {
    const MarkData& m = r.mark;
    out_ << "Submission " << m.submissionId << " | " << r.studentName << " | "
         << r.assignmentTitle << " | raw " << formatMark(m.rawMark) << "/"
         << r.maxMark << " | days late " << m.daysLate << " | final "
         << formatMark(m.finalMark) << " | " << toString(m.status) << "\n"
         << "   feedback: " << m.feedback << "\n";
  }
}

void ConsoleUi::run() {
  out_ << "Moderated LMS\n";
  while (true) {
    std::string username, password;
    if (!readLine("Username (blank to exit): ", username) || username.empty()) {
      return;
    }
    if (!readLine("Password: ", password)) return;
    std::unique_ptr<User> user;
    try {
      user = s_.auth.login(username, password);
    } catch (const Error& e) {
      out_ << "Error: " << e.what() << "\n";
      continue;
    }
    out_ << "Welcome, " << user->fullName() << ".\n";
    try {
      switch (user->role()) {
        case Role::kAdmin: adminMenu(*user); break;
        case Role::kLecturer: lecturerMenu(*user); break;
        case Role::kHod: hodMenu(*user); break;
        case Role::kStudent: studentMenu(*user); break;
      }
    } catch (const Error& e) {
      out_ << "Error: " << e.what() << "\n";
      return;
    }
  }
}

// Every menu has the same shape: pick an item, run it, print any Error and go
// back to the menu. A wrong input never ends the session.
void ConsoleUi::adminMenu(const User& u) {
  const std::vector<std::string> items = {
      "Create user", "List users", "Reset password and unlock",
      "Activate or deactivate user", "Remove a user's personal data",
      "Create department", "Set head of department", "Create course",
      "Assign lecturer to course", "Enrol student", "View audit log",
      "Export audit log", "Back up database", "Restore database",
      "Change my password"};
  while (true) {
    int c = choose("Administrator", items);
    if (c <= 0) return;
    try {
      switch (c) {
        case 1: {
          std::string role = ask("Role (ADMIN, LECTURER, HOD, STUDENT): ");
          std::string name = ask("Username: ");
          std::string full = ask("Full name: ");
          std::string pw = ask("Password: ");
          int id = s_.admin.createUser(u, parseRole(role), name, full, pw);
          out_ << "Created user " << id << ".\n";
          break;
        }
        case 2:
          for (const auto& r : s_.admin.listUsers(u)) {
            out_ << r.id << " | " << r.username << " | " << r.fullName << " | "
                 << toString(r.role) << (r.active ? "" : " | inactive")
                 << (r.failedLogins >= kMaxFailedLogins ? " | locked" : "")
                 << "\n";
          }
          break;
        case 3: {
          int id = askInt("User id: ");
          s_.admin.resetPassword(u, id, ask("New password: "));
          out_ << "Password reset and account unlocked.\n";
          break;
        }
        case 4: {
          int id = askInt("User id: ");
          std::string a = ask("Type ON to activate or OFF to deactivate: ");
          if (a != "ON" && a != "OFF") throw ValidationError("type ON or OFF");
          s_.admin.setActive(u, id, a == "ON");
          out_ << "Done.\n";
          break;
        }
        case 5: {
          int id = askInt("User id: ");
          if (ask("Type YES to remove this person's details: ") != "YES") {
            out_ << "Cancelled.\n";
            break;
          }
          s_.admin.anonymiseUser(u, id);
          out_ << "Personal data removed, marks kept.\n";
          break;
        }
        case 6:
          out_ << "Created department "
               << s_.admin.createDepartment(u, ask("Name: ")) << ".\n";
          break;
        case 7: {
          int d = askInt("Department id: ");
          s_.admin.assignHod(u, d, askInt("HoD user id: "));
          out_ << "Done.\n";
          break;
        }
        case 8: {
          int d = askInt("Department id: ");
          std::string code = ask("Course code: ");
          out_ << "Created course "
               << s_.admin.createCourse(u, d, code, ask("Course name: "))
               << ".\n";
          break;
        }
        case 9: {
          int course = askInt("Course id: ");
          s_.admin.assignLecturer(u, course, askInt("Lecturer user id: "));
          out_ << "Done.\n";
          break;
        }
        case 10: {
          int student = askInt("Student user id: ");
          s_.admin.enrol(u, student, askInt("Course id: "));
          out_ << "Enrolled.\n";
          break;
        }
        case 11:
          for (const auto& e : s_.admin.auditLog(u)) {
            out_ << e.id << " | " << formatTime(e.at) << " | user " << e.actorId
                 << " | " << toString(e.action) << " | " << e.oldValue
                 << " -> " << e.newValue << " | " << e.reason << "\n";
          }
          break;
        case 12:
          s_.admin.exportAudit(u, ask("File name: "));
          out_ << "Exported.\n";
          break;
        case 13:
          requireRole(u, Role::kAdmin);
          s_.db.backupTo(ask("Backup file name: "));
          out_ << "Backup written.\n";
          break;
        case 14:
          requireRole(u, Role::kAdmin);
          if (ask("This replaces all current data. Type YES to go on: ") ==
              "YES") {
            s_.db.restoreFrom(ask("Backup file name: "));
            out_ << "Restored. Log in again.\n";
            return;
          }
          out_ << "Cancelled.\n";
          break;
        case 15: changePassword(u); break;
        default: break;
      }
    } catch (const Error& e) {
      out_ << "Error: " << e.what() << "\n";
    }
  }
}

void ConsoleUi::lecturerMenu(const User& u) {
  const std::vector<std::string> items = {
      "Create assignment", "Give a student an extension",
      "List work waiting to be marked", "Mark a submission",
      "List marks returned to me", "Change my password",
      "Give the whole class an extension"};
  while (true) {
    int c = choose("Lecturer", items);
    if (c <= 0) return;
    try {
      switch (c) {
        case 1: {
          int course = askInt("Course id: ");
          std::string title = ask("Title: ");
          std::string desc = ask("Description: ");
          Time deadline = askTime("Deadline");
          int id = s_.assignments.createAssignment(u, course, title, desc,
                                                   deadline,
                                                   askInt("Maximum mark: "));
          out_ << "Created assignment " << id << ".\n";
          break;
        }
        case 2: {
          int a = askInt("Assignment id: ");
          int st = askInt("Student user id: ");
          Time when = askTime("New deadline");
          s_.assignments.grantExtension(u, a, st, when, ask("Reason: "));
          out_ << "Extension saved.\n";
          break;
        }
        case 3: {
          auto list = s_.marking.unmarked(u);
          if (list.empty()) out_ << "Nothing to mark.\n";
          for (const auto& s : list) {
            out_ << "Submission " << s.id << " | assignment " << s.assignmentId
                 << " | student " << s.studentId << " | "
                 << formatTime(s.submittedAt) << "\n";
          }
          break;
        }
        case 4: {
          int id = askInt("Submission id: ");
          double raw = askNumber("Mark: ");
          MarkData m = s_.marking.mark(u, id, raw, ask("Feedback: "));
          out_ << "Saved. Days late " << m.daysLate << ", final mark "
               << formatMark(m.finalMark) << ".\n";
          break;
        }
        case 5: printMarkRows(s_.marking.returned(u)); break;
        case 6: changePassword(u); break;
        case 7: {
          int a = askInt("Assignment id: ");
          Time when = askTime("New deadline for everyone");
          int n = s_.assignments.extendForClass(u, a, when, ask("Reason: "));
          out_ << "Extension saved for " << n << " student(s).\n";
          break;
        }
        default: break;
      }
    } catch (const Error& e) {
      out_ << "Error: " << e.what() << "\n";
    }
  }
}

void ConsoleUi::hodMenu(const User& u) {
  const std::vector<std::string> items = {
      "List marks waiting for moderation", "Approve a mark", "Adjust a mark",
      "Return a mark to the lecturer", "Release an approved mark",
      "Change my password"};
  while (true) {
    int c = choose("Head of Department", items);
    if (c <= 0) return;
    try {
      switch (c) {
        case 1: printMarkRows(s_.moderation.listPending(u)); break;
        case 2:
          s_.moderation.approve(u, askInt("Submission id: "));
          out_ << "Approved.\n";
          break;
        case 3: {
          int id = askInt("Submission id: ");
          double raw = askNumber("New mark: ");
          s_.moderation.adjust(u, id, raw, ask("Reason: "));
          out_ << "Adjusted and approved.\n";
          break;
        }
        case 4: {
          int id = askInt("Submission id: ");
          s_.moderation.returnMark(u, id, ask("Reason: "));
          out_ << "Returned to the lecturer.\n";
          break;
        }
        case 5:
          s_.moderation.release(u, askInt("Submission id: "));
          out_ << "Released.\n";
          break;
        case 6: changePassword(u); break;
        default: break;
      }
    } catch (const Error& e) {
      out_ << "Error: " << e.what() << "\n";
    }
  }
}

void ConsoleUi::studentMenu(const User& u) {
  const std::vector<std::string> items = {"My assignments", "Submit work",
                                          "My marks", "Change my password"};
  while (true) {
    int c = choose("Student", items);
    if (c <= 0) return;
    try {
      switch (c) {
        case 1: {
          auto list = s_.submissions.assignmentsFor(u);
          if (list.empty()) out_ << "No assignments yet.\n";
          for (const auto& v : list) {
            out_ << v.assignment.id << " | " << v.assignment.title
                 << " | due " << formatTime(v.effectiveDeadline) << " | "
                 << v.status << "\n";
          }
          break;
        }
        case 2: {
          int id = askInt("Assignment id: ");
          std::string work = askWork();
          SubmitResult r = s_.submissions.submit(u, id, work, false);
          if (r.outcome == SubmitResult::Outcome::kNeedsConfirmation) {
            out_ << "This is " << r.daysLate << " day(s) late. "
                 << "The penalty is " << formatMark(r.penaltyOnMax)
                 << " marks off the maximum mark.\n";
            if (ask("Type YES to submit it late: ") != "YES") {
              out_ << "Not submitted.\n";
              break;
            }
            r = s_.submissions.submit(u, id, work, true);
          }
          out_ << (r.replaced ? "Updated. " : "Submitted. ")
               << (r.late ? "Status LATE.\n" : "Status SUBMITTED.\n");
          break;
        }
        case 3: printMarkRows(s_.submissions.releasedMarks(u)); break;
        case 4: changePassword(u); break;
        default: break;
      }
    } catch (const Error& e) {
      out_ << "Error: " << e.what() << "\n";
    }
  }
}

}  // namespace lms
