---
id: TASK-2969
title: 'UX review: the first five minutes of a new user (README and release notes)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T15:37:00Z'
updated_date: '2026-11-27T05:04:00Z'
labels:
  - ux
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1101000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Read-only UX review by otgw-ux-designer, suggested by the coordinator: what a new visitor reads in the first five minutes on the GitHub page (README on the default branch dev, the release list, the newest release notes) and whether they end up with the right firmware. Deliverable: docs/reviews/2026-11-20_ux-first-five-minutes/README.md with findings R1-R7 (severity, effort, evidence, proposed AC); the coordinator triages them (texts: otgw-docs, release-note style: BlueFlame).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Review file lists every finding with heuristic, severity, effort, evidence (file:line or a gh query) and a proposed AC
- [x] #2 Facts checked against the repo and gh (default branch, latest release, release list, settings names); what was not checked is listed under Not verified
- [x] #3 Findings list delivered to the coordinator for triage
- [x] #4 otgw-docs: R2, R3, R5, R6 and R7 of the review are fixed in README.md and the manual (each finding's proposed AC met)
- [ ] #5 BlueFlame: R4, the next beta's release notes have the short head (what, who, known issues, what changed, how to update) and the test provenance as an appendix, and the /beta-release-2x template matches
- [x] #6 Robert: decision on R1 (default branch dev versus a landing text that sends ESP8266 users to /releases/latest) recorded in this task, and the README follows it
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20: review docs/reviews/2026-11-20_ux-first-five-minutes/README.md, findings R1-R7. Facts: gh repo view (default branch dev, latest release v1.7.9 2026-11-04), gh release list (6 newest are 2.0.0 prereleases), settings names checked against v2.js SET_META and the firmware (no HA discovery on/off key, only mqtthaprefix). Line numbers at origin/dev 335e16db7.

2026-11-20: reopened as the umbrella task by coordinator decision. Owners: R2, R3, R5, R6, R7 otgw-docs (passed on by the coordinator); R4 BlueFlame + /beta-release-2x template; R1 to Robert via the coordinator. The review itself (ACs 1-3) is done.

2026-11-20 otgw-docs (95): R2, R3, R5, R6, R7 done in README.md and manual ch03/h03 (MANUAL.md regenerated, test_manual_generated OK). R5: Quick Start moved directly under the intro and beta disclaimer (where R1's firmware choice goes). R2: step 0 hardware check with a link to the ESP8266 migration; step 1 names the erase before the factory command and offers the WiFi update with the release zip for units on 2.0.0. R3: 'Enable HA Discovery' and the 'HA Discovery' rows removed (README step 4 + MQTT table, ch03/h03); checked against v2.js SET_META (only mqtthaprefix) and settingStuff.ino (no discovery on/off): discovery is always on, the prefix must match. R6: no README link into backlog/ (asserted); SAT links go to manual 5.3, 5.21, 5.4 and 5.6 (the manual has no OPV calibration chapter, the backlog doc is from April). R7: example AP name OTGW2-A4:CF:12:21:B4:F8 (_HOSTNAME OTGW2); step 3 names Home and Monitor > Log. R1 and R4 not touched (Robert / BlueFlame).

2026-11-21 otgw-docs (95): AC4 evidence: R2, R3, R5, R6, R7 landed in cffd87b78 (README + manual ch03/h03, MANUAL.md regenerated, test_manual_generated OK; notes above). AC6: Robert chose A for R1 (relayed by the coordinator 2026-11-21): README.md now opens, right under the badges and before the beta disclaimer, with 'Which firmware do you need?': ESP8266 (Wemos D1 mini / NodeMCU) -> https://github.com/rvdbreemen/OTGW-firmware/releases/latest (currently v1.7.10, checked with gh api releases/latest: v1.7.10, 2026-11-20) and the 1.x README on otgw-1.x.x; ESP32-S3 (LOLIN S3 Mini / Pro) or trying 2.0.0 -> read on, with a link to Migrating from ESP8266. The default branch stays dev.

2026-11-27 otgw-ux-designer: recheck written at docs/reviews/2026-11-27_ux-first-five-minutes-recheck/README.md (N1-N5). R1-R3, R5-R7 and the README jargon are fixed. R4 is still open and worse: the beta.451 disclaimer is 37 lines and about 6,600 words, the update instruction is at line 631 of 1034, task numbers appear 166 times in the first 120 lines. AC5 stays unticked (BlueFlame owns it). No beta.452 draft exists yet. Not verified: only head, section list and the update line were read; no reader test.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
First-five-minutes UX review in docs/reviews/2026-11-20_ux-first-five-minutes/README.md: 7 findings R1-R7 with severity, effort, evidence and proposed AC. AC1: the review file. AC2: facts from gh repo view (default branch dev, latest release v1.7.9), gh release list (six 2.0.0 prereleases on top) and the settings names in v2.js SET_META and the firmware; badge value, the 1.x README and the backlog/docs guides' currency listed under Not verified. AC3: findings list sent to the coordinator for triage.
<!-- SECTION:FINAL_SUMMARY:END -->
