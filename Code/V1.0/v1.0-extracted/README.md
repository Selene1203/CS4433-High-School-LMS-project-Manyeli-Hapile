# Moderated Learning Management System

CS4433 Software Engineering, Final Individual Project, National University of Lesotho.

A C++17 / SQLite console application that manages courses, assignments and assessment for a secondary school, with Head-of-Department moderation of marks before release, role-based access (Administrator, Lecturer, Head of Department, Student) and a full audit trail of mark changes.

**Process:** Waterfall with V-Model verification and validation, 7 weeks, 100 marks.

## Where it's at

Weeks 1 to 3 are done. The repo is set up, and the Phase 1 plan, Phase 2 requirements and Phase 3 design documents are in `docs/`. Implementation starts once Gate 2 has passed.

## Build

Requirements: a C++17 compiler (GCC ≥ 9 or Clang ≥ 10), CMake ≥ 3.16, SQLite3 development files (`libsqlite3-dev` on Debian/Ubuntu). GoogleTest is downloaded automatically by CMake.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/lms
```

## Repository layout

```
include/lms/   public headers
src/           implementation
tests/         GoogleTest unit and integration tests
docs/          plans, requirements, design, test and QA documents
.github/       CI workflow (build, test, cppcheck, coverage) and PR template
```

## Process documents
- [Branching strategy](docs/branching-strategy.md)
- [Coding standard](docs/coding-standard.md)
- [Phase 1: Project management plan](docs/phase1/project-management-plan.docx) (the WBS and Gantt chart are Mermaid, source in `docs/phase1/mermaid/`)
- [Phase 2: Requirements engineering](docs/phase2/requirements-engineering.docx) (the use case model is being redone, working files are `use-case-diagram.mmd` and `.drawio` in `docs/phase2/`)
- [Phase 3: System design](docs/phase3/system-design.docx) (diagram code in `docs/phase3/diagrams/`, database in `docs/phase3/db/`)

## Weeks 4 and 5

Implementation and testing write-up: `docs/phase4-5/`. Build and run:

```
cmake -S . -B build && cmake --build build
ctest --test-dir build
./build/lms --db demo.db --demo
```
