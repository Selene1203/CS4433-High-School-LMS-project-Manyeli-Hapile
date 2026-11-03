# Branching Strategy

The project uses **short-lived feature branches merged into `main` by pull request** (GitHub Flow), with **tags marking every phase gate and release**. A single developer works on this repository, so the process is deliberately light, but it is followed consistently so the history is auditable.

## Rules

1. `main` is always buildable and its tests pass. Nothing is committed to `main` directly.
2. Every change starts on a branch from the latest `main` and is merged by pull request. CI (build on GCC and Clang, unit tests, cppcheck) must be green before merge.
3. The pull request template is completed, including the traceability-matrix check.
4. Merges use `--no-ff` (merge commit), so each feature stays visible as a unit in the history.
5. Branches are deleted after merging.

## Branch names

| Prefix | Use | Example |
|---|---|---|
| `feature/` | New functionality tied to a requirement | `feature/late-penalty-strategy` |
| `fix/` | Defect fixes (reference the defect-log ID) | `fix/def-007-extension-rounding` |
| `docs/` | Report, UML and design documents | `docs/phase1-plan` |
| `test/` | Test-only changes | `test/penalty-boundary-values` |
| `release/` | Release stabilisation (v1.0, v1.1) | `release/v1.1` |

## Commit messages

Format: `<type>: <summary in the imperative, ≤72 chars>`, where type is one of `feat`, `fix`, `docs`, `test`, `refactor`, `build`, `ci`, `chore`. The body explains *why*, and references requirement or defect IDs (e.g. `Implements FR-07`).

## Tags

| Tag | Meaning |
|---|---|
| `week-1` … | Weekly checkpoints (lightweight) |
| `gate-1`, `gate-2`, `gate-3` | Phase-gate baselines (annotated) — Requirements, Design, V&V |
| `v1.0` | Initial release (Week 6) |
| `v1.1` | Maintenance release implementing CR1 (Week 7) |

## Change control after a gate

After a gate is passed, its baseline artefacts change only through a pull request that names the reason for the change and updates the traceability matrix and risk register where affected.
