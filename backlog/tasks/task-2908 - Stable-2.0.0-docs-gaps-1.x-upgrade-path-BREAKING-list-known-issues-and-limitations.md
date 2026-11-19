---
id: TASK-2908
title: >-
  Stable 2.0.0 docs gaps: 1.x upgrade path, BREAKING list, known issues and
  limitations
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T07:23:00Z'
updated_date: '2026-11-19T07:29:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 1051000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Gap list for the stable 2.0.0 docs (weekend release). docs-scope-2x.sh --mode stable gives no areas: first v2 stable, no earlier v2 stable tag, so the list is built by hand against the 1.x line. Out of scope: the experimental to opt-in wording (TASK-2034) and the two experimental lines Robert kept (ch05:259/h05:286, draft :196).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ch02:276 and h02:303 no longer claim that MQTT topics and the REST API are unchanged from v1.x; they point to the BREAKING list
- [x] #2 ch02 and h02 have one ordered 1.x to 2.0.0 checklist (save settings, unique id, module swap, USB first flash, restore settings, HA command topic); README Migrating section links to it
- [x] #3 docs/BREAKING_CHANGES.md 2.0.0 section covers the changes against the 1.x line listed in the draft notes, or points to them
- [ ] #4 Draft Known issues curated by a written rule (stays if still true at HEAD; goes only if fixed with evidence); every removal logged with its reason in the notes
- [ ] #5 Draft notes cover beta.375-382 (TASK-2014 2018 2025 2036 2039 2040 2020 2010 1890), decided per item, and have a Limitations block (ESP32-S3 only, no HTTPS, BLE needs PSRAM, firmware SAT vs HA SAT integration, port 25238 filter with SAT on)
- [x] #6 Draft Known issues line SAT is still being validated handled per Robert's answer
- [ ] #7 MANUAL.md rebuilt, test_manual_generated passes, no em dash or TBD in the changed files
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Scope by hand: docs-scope-2x.sh --mode stable gives no areas (first v2 stable). 1.x is measured against the v1.7.9 tag (coordinator OK): on-change default, resetgateway '1' + 5 s, ADR-066 base topic and birth/LWT availability already exist in v1.7.9 (git grep on v1.7.9), so they are not breaking for a 1.x user. Out of scope: experimental wording (TASK-2034) and the kept lines ch05:259/h05:286, draft :196. Coordinator decisions 2026-11-19: id 2908 kept (CLI numbers max+1 over all branches); notes base = draft structure + 'Changes during the 2.0.0 betas' per area, every task in the beta.382 notes one line or on a dropped list here; draft :202 = option A plus the TASK-1371 fact as its own item. Curation rule for Known issues: an item stays if still true at HEAD; it goes only if its fix landed with evidence; every removal is logged here with its reason.
<!-- SECTION:NOTES:END -->
