---
id: TASK-597
title: 'chore(versioning): eliminate per-file version-stamp cascade — version.h as sole source'
status: To Do
assignee: []
created_date: '2026-05-11 17:05'
updated_date: '2026-05-11 17:05'
labels:
  - tech-debt
  - tooling
  - versioning
  - automation
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Each prerelease bump (`bin/bump-prerelease.sh` → `scripts/autoinc-semver.py`) currently rewrites the `**  Version  : v1.5.x-beta.N` header-comment line in every `.ino`, `.h`, `.cpp`, `.css`, `.html`, `.js` file under `src/OTGW-firmware/`. Evidence from the review of the last 100 commits on `dev` (`docs/reviews/2026-05-11_last-100-commits-multi-perspective/REVIEW.md`, section A3):

| Commit | Substantive change | Files touched | Stamp-only lines |
|---|---|---:|---:|
| `503461c8` reset prerelease to beta.1 | none | 26 | 26 (100%) |
| `ee370527` wire `sat/climate_attributes` | 1 source-line | 27 | 25 (93%) |
| `a1a7795e` remove `pressure_health_attr` | 1 block delete | 27 | 25 (93%) |
| `660d4b93` flip Class 1/8 echo flags | 4 OTmap rows | 27 | 23 (85%) |

Cost: `git blame` is dominated by stamp hits, every fix looks ~25× larger than it is, cross-worktree ports (dev ↔ 2.0.0) trigger conflicts on stamps that have zero semantic value. `version.h` already exposes `_VERSION`, `_SEMVER_FULL`, `_VERSION_PRERELEASE` as macros, so the per-file stamps are pure duplication.

Automation requirement: this must not be solved by "remember to remove the stamp from new files" — `scripts/autoinc-semver.py` must be restricted to `version.h` + `data/version.hash` so the cascade cannot recur.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria

<!-- AC:BEGIN -->
- [ ] #1 `scripts/autoinc-semver.py` rewrites only `src/OTGW-firmware/version.h` and `src/OTGW-firmware/data/version.hash`. The `os.walk`-based extension scan (currently at line ~298 with `ext_list = [".h", ".ino", ".cpp", ".c", ".js", ".css", ".html", ".md", ".txt", ".cfg"]`) is removed or restricted to that two-file allowlist.
- [ ] #2 All existing `**  Version  : v` / `//  Version  : v` header-comment lines are removed from files under `src/OTGW-firmware/` (excluding `version.h`). Mechanical one-shot edit via `grep -rln "^\*\*  Version  : v" src/OTGW-firmware/ | xargs sed -i '/^\*\*  Version  : v/d'` (verify the regex against `//  Version  : v` variant too).
- [ ] #3 `.git-blame-ignore-revs` is created at repo root, contains the SHA of the mechanical-delete commit, and is referenced from `CLAUDE.md` and (ideally) a `git config blame.ignoreRevsFile .git-blame-ignore-revs` note.
- [ ] #4 `CLAUDE.md` "Versioning policy" section is updated to state that `src/OTGW-firmware/version.h` is the **sole** source of truth for the version string; per-file `**  Version  :` header stamps are forbidden and `scripts/autoinc-semver.py` will not propagate them.
- [ ] #5 A representative prerelease bump (run `bin/bump-prerelease.sh` on a scratch branch) produces a diff that touches **at most 3 files** (`version.h`, `data/version.hash`, and optionally one `data/version.hash`-adjacent artefact). Capture the `git diff --stat` output in the Final Summary.
- [ ] #6 Build passes (`python build.py --firmware` exits 0) — no source file relies on the deleted header comment for compilation. Evaluator (`python evaluate.py --quick`) shows no new failures.

<!-- AC:END -->

## Definition of Done

<!-- DOD:BEGIN -->
- [ ] #1 ACs above all checked
- [ ] #2 CLAUDE.md updated (AC4) committed in the same PR
- [ ] #3 No new files outside the two-file allowlist still carry a `Version :` comment (grep for residual stamps)
- [ ] #4 Final Summary documents the file count for the next bump (was 25-27, target ≤3)
- [ ] #5 Cross-worktree mirror task opened on 2.0.0 (`feature-dev-2.0.0-otgw32-esp32-sat-support`) if `scripts/autoinc-semver.py` exists there too

<!-- DOD:END -->

## References

- `docs/reviews/2026-05-11_last-100-commits-multi-perspective/REVIEW.md` section A3 + section 8 R2
- `scripts/autoinc-semver.py:181` (`pre_version_text` regex), `:298` (`os.walk`), `:388` (ext_list)
- `bin/bump-prerelease.sh`
- `.githooks/pre-commit` bump-check (must continue to work after change — it only inspects `version.h`, so should be unaffected)
