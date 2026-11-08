---
id: TASK-1725
title: >-
  New worktrees get their git submodules automatically: post-checkout hook plus
  a guard in run_tests and build
status: Done
assignee:
  - '@system32-78'
created_date: '2026-11-08T09:54:00Z'
updated_date: '2026-11-08T17:00:00Z'
labels:
  - tests
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 833000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-08: a fresh git worktree add does not init the submodules (src/libraries/OpenTherm, src/libraries/SimpleTelnet, other-projects), so C++ host harnesses fail on a missing OpenTherm.h (system32-78 report) and other-projects is empty, which breaks the SAT ground-truth and PIC reference reads. .gitmodules also lists Arduino/libraries/aceTime and Arduino/libraries/Time, which are not initialised in the main tree either.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A new worktree via git worktree add has every needed submodule initialised at its recorded commit, without a manual step: .githooks/post-checkout runs git submodule update --init on a worktree's first checkout; hooksPath already .githooks
- [x] #2 scripts/run_tests.py and build.py stop early with one clear message naming the missing submodule and the command to fix it, instead of compiler errors
- [x] #3 Decision recorded for Arduino/libraries/aceTime and Time: still needed or removed from .gitmodules, with evidence
- [x] #4 docs/guides/worktrees.md describes the behaviour; red-green evidence: fresh worktree before the fix has empty submodule dirs, after the fix populated; run_tests.py guard shown failing and passing
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 implementation and evidence. (1) .githooks/post-checkout (100755): on a worktree's first checkout (prev HEAD = null ref) runs git submodule update --init for every submodule with a gitlink in the commit, borrowing objects from the main worktree's copy via --reference --dissociate when that copy is a full clone; the main tree's OpenTherm is shallow and git refuses a shallow reference (first probe: 'reference repository ... is shallow', OpenTherm stayed empty), so a shallow copy is fetched from its URL instead. Never fails the checkout; prints the fix command. Bypass OTGW_SUBMODULE_HOOK_DISABLE=1. Red: fresh git worktree add of origin/dev without the hook: other-projects=0, OpenTherm=0, SimpleTelnet=0 entries. Green: git -c core.hooksPath=<this tree>/.githooks worktree add: other-projects=19, OpenTherm=8, SimpleTelnet=14 entries, git submodule status at the recorded commits c5211afee / f7d82544a / a1067d4ec, 89 s; other-projects has no alternates afterwards (dissociated). A later branch switch in that worktree ran no hook. A CRLF copy of the hook runs under Git for Windows bash (same as the existing CRLF pre-commit). (2) scripts/submodule_check.py + calls in scripts/run_tests.py (after parse_args, before selection; skipped for --list) and build.py main() (before the build lock): OpenTherm/SimpleTelnet missing -> exit 2 with 'Missing git submodule(s): ... Fix: git submodule update --init -- <paths>'; other-projects missing -> note only in run_tests (tests skip; CI has no access, build.yml). Red: in the empty worktree run_tests selected 189 tests with exit 0; green: run_tests and build.py both exit 2 with that message there, and pass in the populated worktree. tests/test_submodule_check.py 4/4 OK (failed on import before the helper existed). (3) Decision: Arduino/libraries/aceTime and Arduino/libraries/Time removed from .gitmodules: no gitlink on origin/dev or origin/otgw-1.x.x (both record only other-projects, OpenTherm, SimpleTelnet); never initialised in the main tree; dead since 867432a90 (2023-02-11, 'Removing git submodules from src tree'); AceTime comes from platformio.ini lib_deps (bxparks/AceTime.git#v2.0.1), Time is unused, lib_extra_dirs is src/libraries. (4) docs/guides/worktrees.md and .githooks/README.md describe the hook, the guard and the hooksPath caveat (worktrees use the main tree's .githooks, so the hook acts once the main tree has this commit). evaluate --quick 0 failed. Not done: a hook run on Linux/macOS.

Adopted by system32-78 2026-11-08 (toegang's session gone). Re-verified 8bf84c51a: gitlinks in the tree are only other-projects, src/libraries/OpenTherm, src/libraries/SimpleTelnet, so the removed .gitmodules entries (Arduino/libraries/aceTime, Time) were stale. tests/test_submodule_check.py 4/4 OK; evaluate.py --quick 0 FAIL 0 WARN. Fresh worktree with the repo's core.hooksPath (absolute path to the MAIN tree's .githooks, which does not have post-checkout yet): 0/0/0 submodule files, i.e. the hook is inert until the main tree's checkout contains it (as docs/guides/worktrees.md says). Same fresh worktree with git -c core.hooksPath=<wt1725>/.githooks: OpenTherm 10, SimpleTelnet 20, other-projects 26503 files. Guard: submodule_check.check() on an empty tree prints the missing OpenTherm + SimpleTelnet and the fix command, exit 2. Open: one esp32-combo build in a populated tree (guard passes), then rebase + push; afterwards the main tree must be fast-forwarded for the hook to go live.

Proving build 2026-11-08 (system32-78): esp32-combo beta.149+3dc9dfa built from wt-1716 (populated tree, submodules initialised) together with TASK-1716: build.bat exit 0, artifacts 17:42-17:43; the build.py submodule guard passed. Rebased onto origin/dev 26e00de54 as 6a2cf501b; conflict in docs/guides/worktrees.md with TASK-1723's manual note resolved by keeping this section and adding that note as a bullet (manual init until the hook is live, fresh worktree needs its own .build-venv). After the push the coordinator fast-forwards the main tree so the hook goes live.
<!-- SECTION:NOTES:END -->
