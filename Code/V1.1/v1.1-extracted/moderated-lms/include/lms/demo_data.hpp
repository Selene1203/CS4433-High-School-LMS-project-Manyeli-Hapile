#pragma once

#include "lms/sqlite_repository.hpp"

namespace lms {

/// Fills an empty database with a small school for the demo and the viva:
/// one department, one course, one of each role, two assignments, and a few
/// submissions, one of them late. Logins are printed by main.
void loadDemoData(SqliteRepository& repo);

/// First start on an empty database: makes one administrator.
void createFirstAdmin(SqliteRepository& repo, const std::string& username,
                      const std::string& password);

}  // namespace lms
