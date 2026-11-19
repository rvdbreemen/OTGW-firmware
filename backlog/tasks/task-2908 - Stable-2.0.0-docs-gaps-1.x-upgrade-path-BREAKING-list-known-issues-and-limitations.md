---
id: TASK-2908
title: >-
  Stable 2.0.0 docs gaps: 1.x upgrade path, BREAKING list, known issues and
  limitations
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T07:23:00Z'
updated_date: '2026-11-19T07:42:00Z'
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
- [x] #4 Draft Known issues curated by a written rule (stays if still true at HEAD; goes only if fixed with evidence); every removal logged with its reason in the notes
- [x] #5 Draft notes cover beta.375-382 (TASK-2014 2018 2025 2036 2039 2040 2020 2010 1890), decided per item, and have a Limitations block (ESP32-S3 only, no HTTPS, BLE needs PSRAM, firmware SAT vs HA SAT integration, port 25238 filter with SAT on)
- [x] #6 Draft Known issues line SAT is still being validated handled per Robert's answer
- [x] #7 MANUAL.md rebuilt, test_manual_generated passes, no em dash or TBD in the changed files
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Scope by hand: docs-scope-2x.sh --mode stable gives no areas (first v2 stable). 1.x is measured against the v1.7.9 tag (coordinator OK): on-change default, resetgateway '1' + 5 s, ADR-066 base topic and birth/LWT availability already exist in v1.7.9 (git grep on v1.7.9), so they are not breaking for a 1.x user. Out of scope: experimental wording (TASK-2034) and the kept lines ch05:259/h05:286, draft :196. Coordinator decisions 2026-11-19: id 2908 kept (CLI numbers max+1 over all branches); notes base = draft structure + 'Changes during the 2.0.0 betas' per area, every task in the beta.382 notes one line or on a dropped list here; draft :202 = option A plus the TASK-1371 fact as its own item. Curation rule for Known issues: an item stays if still true at HEAD; it goes only if its fix landed with evidence; every removal is logged here with its reason.

Known issues curation (for release-2x checkpoint 2). REMOVED 9, each fix on origin/dev (merge-base checked) with evidence in the task final summary: TASK-1919 x2 (first-boot rollback; 0d0606972, old-vs-fix bench on .61 and .63); TASK-1943 (PR= answers; 3556d199e, 15/15 on .63); TASK-1925 (Data-Invalid cached; a762f42b6, host red/green, boiler side stays under the OT-Direct items); TASK-1937 (graph weather value; 0f52279e9, red/green on .61); TASK-1763 (25 s reboot; d4f9c627d ADR-225, 0 panics in 150 reboots, least clear-cut: low-heap boot clause waived by Robert); TASK-1750 (MsgID 100 flags; 43db6fda1, 46/46 host, real thermostat read stays under the gateway-mode item); TASK-1751 (TT=0; 43db6fda1, bench .61); TASK-1867 pump-head step (c7dd96dd0, replay red/green; real sensor stays under the pressure item). REWORDED 10: TASK-1979, 1913 (42/42 USB resets now benched), 1865/1899, 1741, 1703, 1675, 1678, 1645/1455 (the beta AP fallback does not exist in stable: networkStuff.ino:497 _VERSION_PRERELEASE), 1528, 1584-1588. KEPT 42. Table: scratchpad known-issues-curation.md (agent proposal, removals verified by me).

Beta history coverage: 'Changes during the 2.0.0 betas' has 103 bullets; all 112 TASK ids in the beta.382 'What changed since' sections are in it or dropped (coverage script: 112 in, 0 missing). Dropped 12: TASK-1942, 1874, 1866, 2000, 1979, 1665 (in Highlights); 1889 (in 'For users of a 2.0.0 alpha or beta'); 1695 (in the SAT section); 2026 (open follow-up, nothing shipped); 2021 (docs only); 1894 (build tooling); 2019 (diagnostics only). TASK-1792 line corrected to counted hours (TASK-1889).

Extra findings fixed: (1) the v2 web UI has no Wi-Fi scan panel, ch03/h03 and ch09/h09 said it had (v2.js never calls /api/v2/network/scan; commit ce395f868). (2) AP fallback exists only in beta builds (networkStuff.ino:497), ch01/h01, ch02/h02 and ch07/h07 presented it unqualified; h07 also called the config-portal log line 'AP fallback'. (3) The draft 'Compared with the 1.x line' is split into 'Compared with 1.x (v1.7.9)' and 'For users of a 2.0.0 alpha or beta'.
<!-- SECTION:NOTES:END -->
