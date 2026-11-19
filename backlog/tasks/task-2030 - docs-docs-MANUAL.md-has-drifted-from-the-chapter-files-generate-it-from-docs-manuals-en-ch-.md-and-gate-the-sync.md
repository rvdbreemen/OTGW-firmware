---
id: TASK-2030
title: >-
  docs: docs/MANUAL.md has drifted from the chapter files; generate it from
  docs/manuals/en/ch*.md and gate the sync
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T14:52:00Z'
updated_date: '2026-11-18T15:46:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 1041000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-18 (otgw-docs). docs/MANUAL.md is a hand-kept single-file copy of the EN chapters with its own section numbers (factory reset is 7.10 there, 7.13 in ch07). It has 3866 lines vs 5758 in ch01-ch10 and lacks Board mode, bootdetect.log, the BLE PSRAM-aware default, 7.12a recovery and the sun-elevation rule; MANUAL.md:1521 still says /settings.json. 387 non-empty lines exist only in MANUAL.md, across 126 headings (e.g. 'Release artefact verification', 'Migrating from 1.x'): those must be reviewed before any regeneration so no content is lost. tests/test_manual_sat_commands.py and tests/test_sat_mqtt_state.py read MANUAL.md; scripts/docs-scope-2x.sh lists it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every MANUAL-only line group is either folded into the matching chapter (current and verified against code) or recorded as dropped with the reason
- [x] #2 A script builds docs/MANUAL.md from the header plus docs/manuals/en/ch01..ch10 in order; the committed file equals its output
- [x] #3 A test fails when docs/MANUAL.md differs from the generated output (red on today's file, green after)
- [x] #4 tests/test_manual_sat_commands.py and tests/test_sat_mqtt_state.py stay green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 review of the 387 MANUAL-only lines (126 headings; review file in the otgw-docs scratchpad, manual-only.md). FOLDED into the chapters, verified first: (1) 'Release artefact verification' -> ch02/h02 new section; checked on the real v2.0.0-beta.360 release: SHA256SUMS is an asset and sha256sum -c accepts it (ino.bin OK); added --ignore-missing (a partial download otherwise prints FAILED for absent files, shown); macOS shasum line checked with the Perl shasum only, not on a Mac. (2) BLE trust note -> ch05/h05 5.16, rewritten to the current code: 8-slot roster (SATtypes.h:509), auto-select of the only fresh sensor (SATble.ino:768-795), no connect or write to a sensor (no connect/createClient in SATble.ino).

DROPPED as stale against the code: 'Debugging the gate' (telnet key g: no case 'g' in handleDebug.ino; bMQTTGate is only set on prerelease builds, debugStuff.ino:119); BLE 'up to four sensors / four slots' (roster is 8, SATtypes.h:509); SAT 5.7 'Boot safety: CS=0 before the first cycle' (entry 129: no SAT command at boot); 'Auto-switch' wording of 5.6 (chapter 5.6 is current, ADR-249/TASK-2005); 7.9 'Exception (2)/(3)' rows (ESP8266 exception codes, not the S3); 6.9 'WebSocket live log (port 81)' (it is /ws on port 80, webSocketStuff.ino:39); 7.10 Factory Reset '/settings.json' (SETTINGS_FILE is /settings.ini, OTGW-firmware.h:138).

DROPPED as an older wording of a section the chapter keeps current (same heading in ch01-ch10, the chapter text is the maintained one): the remaining groups under Chapters 1, 3, 4, 5, 6, 7 (What Is New, Highlights, Navigation, Log Viewer, Temperature Graph, Network Status, Wi-Fi, FSexplorer, Theme, HA Overview/Discovery/Climate/SAT entities, 5.1-5.11, 6.1.2, 6.5-6.9, 7.1-7.8). 'Triggering a full rediscovery manually' is in ch04:241-250. 1.x migration: 'Going back to 1.x', the API v0/v1 note and the pin-number change are in ch02:20, :116-130, :276-279. TO ROBERT before any drop (coordinator condition 2): the module table 'NodeMCU for OTGW 1.x through 2.0, Wemos D1 mini for 2.3 and later' (MANUAL.md:163-168) is hardware history that the code cannot confirm and is in no chapter.

2026-11-18 generator: scripts/build_manual.py builds docs/MANUAL.md = header + ch01..ch10 joined by '---', relative chapter links rebased from docs/manuals/en/ to docs/ (--check exits 1 on a mismatch). tests/test_manual_generated.py: on the hand-kept MANUAL.md -> FAILED (test_matches_generated, test_check_flag_exit_code); after python scripts/build_manual.py ('wrote docs/MANUAL.md (5813 lines from 10 chapters)') -> Ran 4 OK, incl. every relative link resolving from docs/. Unchanged green: test_manual_sat_commands OK, test_sat_mqtt_state OK. Flow (coordinator condition 1): no skill or script wrote MANUAL.md (update-docs-2x edits chapters only; docs-scope-2x.sh only counts mentions in it), so update-docs-2x Phase 5 step 2 now runs build_manual.py + the test when an EN chapter changed. Generated MANUAL.md: Board mode 5, bootdetect.log 4, 7.12a 3, settings.json 0, em dashes 0. AC1 stays open: the NodeMCU/Wemos module table (1.x history, MANUAL.md:163-168) is with Robert; the generated file drops it, so this commit is NOT pushed until he answers (keep: fold into ch02/h02; drop).

2026-11-18 Robert on the NodeMCU/Wemos module table (MANUAL.md:163-168, the only MANUAL-only block about 1.x migration): "B" = drop it (Discord DM 1572638479970467982, relayed by the coordinator). The generated MANUAL.md already leaves it out. AC1 complete: every MANUAL-only group folded (release verification, BLE trust) or dropped with its reason in these notes. Push: cca4d660c with tonight's docs push; after the rebase onto the release commit, rerun python scripts/build_manual.py and python tests/test_manual_generated.py before the push (the release commit may touch chapters).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
docs/MANUAL.md is now generated from docs/manuals/en/ch01..ch10 by scripts/build_manual.py, chapter links rebased to docs/. AC1: 387 MANUAL-only lines under 126 headings reviewed; folded and verified: release artefact verification (ch02/h02, checked on the real v2.0.0-beta.360 SHA256SUMS) and a current BLE trust note (ch05/h05); the rest dropped with a reason per group (stale vs code, or an older wording of a maintained chapter section); the NodeMCU table dropped on Robert's "B" (DM 1572638479970467982). AC2: generator committed, MANUAL.md equals its output. AC3: tests/test_manual_generated.py red on the hand-kept file, 4/4 green after the rebuild. AC4: test_manual_sat_commands and test_sat_mqtt_state OK. /update-docs-2x Phase 5 rebuilds it. Not verified: a rendered view of MANUAL.md on GitHub.
<!-- SECTION:FINAL_SUMMARY:END -->
