---
id: TASK-2963
title: >-
  UX review of the tester CLI tools: capture-mqtt-debug and flash_otgw (Windows,
  Linux, macOS)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T15:11:00Z'
updated_date: '2026-11-20T15:25:00Z'
labels:
  - ux
  - tools
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1096000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Read-only UX review by otgw-ux-designer, assigned by the coordinator: first-use flow, prompts, error messages, what a tester sees when something fails, and Windows vs Linux/macOS parity for scripts/capture-mqtt-debug.{bat,sh}, scripts/capture-mqtt-debug-macos.sh, scripts/otgw_capture.py and flash_otgw.{bat,sh}. Nothing is run against a unit. Deliverable: docs/reviews/2026-11-20_ux-cli-tools/README.md with findings (severity, effort, evidence file:line, proposed AC); the coordinator triages them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Review file lists every finding with heuristic, severity 0-4, effort S/M/L, evidence (file:line) and a proposed AC
- [x] #2 Every claim is from reading the scripts (or a dry run without a unit, such as --help); anything not verifiable read-only is listed under Not verified
- [x] #3 Windows vs Linux/macOS parity is compared per step (launch, Python, prompts, stop, upload, errors)
- [x] #4 Findings list delivered to the coordinator for triage
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20: review docs/reviews/2026-11-20_ux-cli-tools/README.md, 9 findings C1-C9 (sev, effort, file:line, proposed AC). Read-only: only otgw_capture.py --help was run (111 lines, no network); no unit touched. Parity compared per step in each finding (launch, Python bootstrap, prompts, stop, upload, errors, AP name, mode header). Line numbers checked against origin/dev 25d13036f after the rebase.

2026-11-20: findings list C1-C9 sent to the coordinator for triage (message with sev, effort, evidence and AC per finding).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Read-only UX review of the tester CLI tools in docs/reviews/2026-11-20_ux-cli-tools/README.md: 9 findings C1-C9, each with severity, effort, file:line and a proposed AC. AC1: the review file. AC2: everything read from the scripts at origin/dev 25d13036f; only otgw_capture.py --help was run (111 lines, no network); real runs, Defender behaviour and esptool error output listed under Not verified. AC3: parity per step in the findings (launch and Python bootstrap, prompts, stop, upload, error paths, setup-AP name, mode header). AC4: findings list sent to the coordinator. Top items: C1 factory flash without confirmation, C3 wrong setup-AP name in both flash tools, C2 double-click windows close, C4 long run not reachable interactively, C5 esptool via PowerShell Bypass without a hash.
<!-- SECTION:FINAL_SUMMARY:END -->
