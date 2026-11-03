#pragma once

#include <stdexcept>
#include <string>

namespace lms {

/// Base class for every error the system reports on purpose.
class Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// Bad input from a user (empty text, mark out of range, bad date).
class ValidationError : public Error {
 public:
  using Error::Error;
};

/// The logged-in role is not allowed to do this (FR-02).
class PermissionError : public Error {
 public:
  using Error::Error;
};

/// A business rule (BR1 to BR5) says no.
class RuleError : public Error {
 public:
  using Error::Error;
};

/// Something the caller asked for does not exist.
class NotFoundError : public Error {
 public:
  using Error::Error;
};

/// SQLite reported a failure.
class DatabaseError : public Error {
 public:
  using Error::Error;
};

}  // namespace lms
