---
id: TASK-314
title: '[SEC-L2] Randomize or remove AP fallback password before 2.0.0 GA'
status: Done
assignee: []
created_date: '2026-04-18 19:23'
updated_date: '2026-04-19 07:12'
labels:
  - security
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
networkStuff.ino:232 hard-codes AP password otgw123 inside _VERSION_PRERELEASE gate. WiFiManager already handles first-boot provisioning. Anyone in RF range with prerelease build can associate.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 AP fallback either removed entirely or password randomized per device
- [ ] #2 Randomization derived from chipId with a build-time salt and surfaced on OLED/serial only
- [ ] #3 evaluate.py refuses to build a non-prerelease tag if _VERSION_PRERELEASE paths remain reachable
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision 2026-04-19 by Robert: AP fallback with fixed password otgw123 is a deliberate development aid, not a bug. The _VERSION_PRERELEASE gate already keeps it out of production builds; the fixed password is intentional so beta testers have a consistent debug entry point when WiFi provisioning fails. Security review noted it as informational only; no code change needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as won't-fix per user decision. Fixed-password AP fallback is a deliberate beta-debug feature, already gated behind _VERSION_PRERELEASE so it cannot ship in production. Reviewer flagged it as informational only; no action required.
<!-- SECTION:FINAL_SUMMARY:END -->
