---
id: TASK-638
title: Document remaining unrecorded commits since v1.5.0-fix2 in README + CHANGELOG
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-20 12:23'
updated_date: '2026-05-20 12:23'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-637. Three commits since v1.5.0-fix2 were not captured in the README dev section or CHANGELOG Unreleased: (a) PR #581 (docs review findings 1-5 + link-check workflow); (b) PR #574 (brief main-branch styling slip on dev README, later corrected by #581); (c) TASK-596 docs update (MQTT.md JIT discovery semantics, docs/api/README.md /discovery/verify fix, docs/adr/README.md ADR-041/ADR-073 index entries, release housekeeping moving RELEASE_NOTES_1.5.0 out of repo root and archiving 1.3.3/1.3.4). Add bullets under the existing Unreleased Documentation section and add brief CHANGELOG notes for the dev-banner regression and the link-check CI workflow.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CHANGELOG.md Unreleased Documentation section gains a bullet for the TASK-596 docs update (MQTT.md JIT semantics, /discovery/verify endpoint fix, ADR README index ADR-041/ADR-073 entries)
- [ ] #2 CHANGELOG.md Unreleased Documentation section gains a bullet for the release housekeeping (RELEASE_NOTES_1.5.0 moved from repo root to docs/releases/; 1.3.3/1.3.4 archived to docs/releases/archive/)
- [ ] #3 CHANGELOG.md Unreleased Documentation section gains a bullet for the docs-review findings 1-5 fix (#581) — stale ../ link paths corrected across BUILD.md, FLASH_GUIDE_NL.md, PIC_FIRMWARE_EN.md, browser-debug-console.md, DOCUMENTATION_LINKS_POLICY.md
- [ ] #4 CHANGELOG.md Unreleased Changed or Added section reflects the link-check job added to .github/workflows/evaluate.yml (#581)
- [ ] #5 CHANGELOG.md Unreleased Documentation section notes the dev README banner restore in #581 after a brief main-branch styling slip in #574
- [ ] #6 README dev section What's new on dev mentions API/ADR/release-notes housekeeping in the Documentation block
- [ ] #7 No em dashes in any new prose (use colons, periods, commas, parentheses)
- [ ] #8 Existing CHANGELOG and README content remains intact; this update is purely additive (or surgically corrective on the slip narrative)
<!-- AC:END -->
