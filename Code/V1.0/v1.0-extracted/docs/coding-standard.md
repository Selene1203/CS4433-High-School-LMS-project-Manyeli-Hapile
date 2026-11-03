# Coding Standard

Based on the **C++ Core Guidelines** and the **Google C++ Style Guide** formatting (enforced by `.clang-format`), with the project-specific rules below. Static analysis (`clang-tidy`, `cppcheck`) enforces what can be automated.

## Language and build
- C++17, no compiler extensions. Builds warning-free with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wold-style-cast`.
- Build only through CMake. Third-party dependencies are declared in `CMakeLists.txt` with pinned versions.

## Naming
| Item | Convention | Example |
|---|---|---|
| Types, classes | `PascalCase` | `LatePenaltyStrategy` |
| Functions, methods | `camelCase` | `computePenalty()` |
| Variables, parameters | `camelCase` | `daysLate` |
| Private data members | `camelCase_` (trailing underscore) | `maxMark_` |
| Constants, enumerators | `kPascalCase` | `kMaxLateDays` |
| Namespaces | `lowercase` | `lms::data` |
| Files | `snake_case.hpp/.cpp` | `late_penalty_strategy.hpp` |

## Design and safety rules
1. **Ownership:** RAII everywhere; no raw `new`/`delete`; use `std::unique_ptr`/`std::shared_ptr`/values. No owning raw pointers.
2. **Errors:** report failures with exceptions derived from `lms::Error`; validate all external input at the boundary (UI and data-access layer). Never let an exception escape `main()`.
3. **Database access:** only through the data-access layer (repository classes). All SQL uses **parameterised statements**; never build SQL by string concatenation.
4. **Passwords:** never stored or logged in plain text; salted hashes via a recognised library only.
5. **Const-correctness:** `const` by default; mark single-argument constructors `explicit`; use `override` and `final`.
6. **Interfaces:** abstract base classes have virtual destructors and no data members.
7. **Layering:** UI → services (business logic) → repositories → SQLite. A lower layer never includes a higher layer's headers.
8. **No magic numbers:** policy values (e.g. 10% per day, 3 days) live in named constants or configuration.
9. **Functions** do one thing, ≤ 40 lines, cyclomatic complexity ≤ 10 (measured; exceptions justified in review).
10. **Comments** explain *why*, not *what*. Public headers carry short Doxygen-style `///` comments.

## Review
Every pull request is self-reviewed against the code-review checklist (added in Week 4) and must pass CI, including static analysis, before merge.
