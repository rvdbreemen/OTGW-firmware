---
id: TASK-1994
title: Public beta v2.0.0-beta.339 (2026-11-16 08:00 slot)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T05:12:00Z'
updated_date: '2026-11-16T05:18:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1020000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta for the 08:00 slot via /beta-release-2x (Robert's standing permission 2026-11-14). Candidate: origin/dev HEAD at T-75 (06:45), firmware beta.339 (92fc8fec7) unless newer lands. Release commit is docs-only: notes, CHANGELOG rollover (with the missing lines for TASK-1919, 1931, 1932, 1972, 1986, 1988 and the TASK-1980 line moved out of the beta.323 section), README. Composed by comms session 92.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit pushed at T-75 with notes listing every change since beta.323, TASK-1986 and TASK-1988 ancestors of it
- [ ] #2 Release build SUCCESS, sha256 recorded, 1 h soak on .63 without restart (bootcount = flash + 2, no crashlog)
- [ ] #3 Tag on the release commit, draft then publish with --prerelease --latest=false, releases/latest stays v1.7.9, download sha256 equals the build
- [ ] #4 #beta-testing post with every change since beta.323 (numbered parts < 2000), read back; #alpha-testing note; gruijter notified about the capture changes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Prep 06:00-06:30 (92): notes docs/releases/RELEASE_NOTES_2.0.0-beta.339.md built from the beta.323 file: new section 'What changed since beta.323' (16 builds beta.324-339 + capture tool TASK-1972/1986/1988), disclaimer bullet, BREAKING 'New from beta.323 to beta.339', Known issues (TASK-1981 issue removed as fixed; added 1979 HA screenshot/Classic, 1990 no real burner, 1867 no pressure sensor, 1919 no old-vs-fix bench). Lint: no TODO/em dash/relative links/OTGW32; 116887 B < 125000 (body fits). CHANGELOG: [Unreleased] rolled to [2.0.0-beta.339] - 2026-11-16; lines added for TASK-1919, 1931, 1932, 1972, 1986, 1988; TASK-1980 line moved from the beta.323 section (commit 9918e9b88 is after the tag); TASK-1867 line amended with entry 372. Cross-check vs git log v2.0.0-beta.323..origin/dev -- src/ scripts/: all 16 task ids present. README -> beta.339.

Host-suite evidence for the release HEAD (e255ab20b src): 92 swept 115 harnesses that compile SAT/settings code in 5 foreground chunks (113 PASS; test_sat_pm25_poll passes alone, 16 OK); ba ran test_sat_lowload_contract on bbe1cc838 (same src) in 5 foreground chunks: all 18 LL ids PASS (LL-016 skip by design). test_sat_pid_resets fixed in ba861f1cd (red C3861 on the old harness, 17 OK with the stub).
<!-- SECTION:NOTES:END -->
