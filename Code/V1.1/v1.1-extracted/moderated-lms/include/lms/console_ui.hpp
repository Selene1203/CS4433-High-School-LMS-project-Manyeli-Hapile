#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "lms/database.hpp"
#include "lms/services.hpp"

namespace lms {

/// Everything the menus need. They only call services, never repositories.
struct Services {
  AuthService& auth;
  AdminService& admin;
  AssignmentService& assignments;
  SubmissionService& submissions;
  MarkingService& marking;
  ModerationService& moderation;
  Database& db;
  std::string dbPath;
};

/// Plain text menus. Each role only sees what it can use (NFR-07), and a bad
/// input prints a message and asks again, it never stops the program (NFR-05).
class ConsoleUi {
 public:
  ConsoleUi(Services services, std::istream& in, std::ostream& out)
      : s_(services), in_(in), out_(out) {}

  /// Login loop. Returns when the user leaves at the login prompt or the
  /// input ends.
  void run();

 private:
  /// Opens the menu that belongs to the role of the user.
  void menuFor(const User& user);
  struct MenuItem {
    std::string label;
    std::function<void()> action;
  };
  void runMenu(const std::string& title, const std::vector<MenuItem>& items);
  void showAssignments(const User& user);
  void submitWork(const User& user);
  void adminMenu(const User& user);
  void lecturerMenu(const User& user);
  void hodMenu(const User& user);
  void studentMenu(const User& user);

  bool readLine(const std::string& prompt, std::string& line);
  std::string ask(const std::string& prompt);
  int askInt(const std::string& prompt);
  double askNumber(const std::string& prompt);
  Time askTime(const std::string& prompt);
  std::string askWork();
  /// Prints the menu, returns the choice, or -1 when the input ended.
  int choose(const std::string& title, const std::vector<std::string>& items);
  void changePassword(const User& user);
  void printMarkRows(const std::vector<MarkRow>& rows);

  Services s_;
  std::istream& in_;
  std::ostream& out_;
  bool leaveMenu_ = false;  // set by an action that ends the menu
};

}  // namespace lms
