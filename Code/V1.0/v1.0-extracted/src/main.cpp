#include <iostream>
#include <string>

#include "lms/console_ui.hpp"
#include "lms/demo_data.hpp"
#include "lms/error.hpp"
#include "lms/sqlite_repository.hpp"
#include "lms/version.hpp"

namespace {

void usage() {
  std::cout << "Usage: lms [--db FILE] [--demo] [--version]\n"
               "  --db FILE   database file, default lms.db\n"
               "  --demo      fill a new database with sample data\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string path = "lms.db";
  bool demo = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--version") {
      std::cout << "Moderated LMS " << lms::appVersion() << " (SQLite "
                << lms::sqliteVersion() << ")\n";
      return 0;
    }
    if (a == "--demo") {
      demo = true;
    } else if (a == "--db" && i + 1 < argc) {
      path = argv[++i];
    } else {
      usage();
      return 2;
    }
  }
  try {
    lms::Database db(path);
    lms::SqliteRepository repo(db);
    if (repo.listUsers().empty()) {
      if (demo) {
        lms::loadDemoData(repo);
        std::cout << "Demo data loaded. Logins (username / password):\n"
                     "  admin / Admin#2026\n  hod1 / Hod#2026pw\n"
                     "  lect1 / Lect#2026pw\n  stu1 / Stud#2026pw\n"
                     "  stu2 / Stud#2026pw\n";
      } else {
        std::string password;
        std::cout << "New database. Choose a password for the first "
                     "administrator ('admin'): ";
        std::getline(std::cin, password);
        lms::createFirstAdmin(repo, "admin", password);
      }
    }
    lms::StandardPenaltyStrategy penalty;
    lms::Clock clock = lms::systemNow;
    lms::AuthService auth(repo, clock);
    lms::AdminService admin(repo, repo, repo, clock);
    lms::AssignmentService assignments(repo, repo, repo, repo, repo, repo, clock);
    lms::SubmissionService submissions(repo, repo, repo, repo, penalty, clock);
    lms::MarkingService marking(repo, repo, repo, repo, repo, repo, penalty,
                                clock);
    lms::ModerationService moderation(repo, repo, repo, repo, repo, penalty,
                                      clock);
    lms::ConsoleUi ui({auth, admin, assignments, submissions, marking,
                       moderation, db, path},
                      std::cin, std::cout);
    ui.run();
  } catch (const lms::Error& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Unexpected error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
