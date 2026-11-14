---
id: TASK-1929
title: 'CI: the firmware build workflow compiles esp32-combo only'
status: Done
assignee: []
created_date: '2026-11-14T08:47:00Z'
updated_date: '2026-11-14T09:14:00Z'
labels:
  - ci
dependencies: []
ordinal: 987000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-14: 'Remove the remote CI for building all firmwares remotely. Just build combo.' .github/workflows/build.yml ran a matrix of esp32, esp32-classic and esp32-combo on every PR and every non-docs push to dev (TASK-1626). From now on it compiles esp32-combo only; esp32 and esp32-classic are no longer built remotely (locally: build --target all).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.yml matrix holds esp32-combo only; the header comment says so
- [x] #2 A push of the change shows one build job (pio run -e esp32-combo) on GitHub Actions, green
- [x] #3 Docs that describe what CI builds are updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 pushed c5af78410 (ff5eb599e..c5af78410) on Robert's 'Push maar' (the push request to the coordinator expired undelivered: permission modes differ). Evidence: GitHub Actions run 43579336065 'PlatformIO firmware build' on c5af78410 had exactly one job, 'pio run -e esp32-combo', success (09:11:09-09:13:28 UTC); evaluate.py CI gates success on the same commit. evaluate --quick locally: 92 checks, 0 FAIL. dev has no branch protection, so no required check named the removed esp32/esp32-classic jobs. docs/guides/BUILD.md CI paragraph now names build.yml (it named a non-existent main.yml). Not changed: dependency-scan.yml still scans all three envs (pio pkg outdated/list), a scan, not a build; Robert to decide. Rest of BUILD.md 'What the Build Script Does' (arduino-cli, Makefile) is stale, out of scope.
<!-- SECTION:NOTES:END -->
