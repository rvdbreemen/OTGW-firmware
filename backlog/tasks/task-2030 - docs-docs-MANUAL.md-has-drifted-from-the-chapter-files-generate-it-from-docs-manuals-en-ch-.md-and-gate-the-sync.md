---
id: TASK-2030
title: >-
  docs: docs/MANUAL.md has drifted from the chapter files; generate it from
  docs/manuals/en/ch*.md and gate the sync
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T14:52:00Z'
updated_date: '2026-11-18T15:02:00Z'
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
- [ ] #1 Every MANUAL-only line group is either folded into the matching chapter (current and verified against code) or recorded as dropped with the reason
- [ ] #2 A script builds docs/MANUAL.md from the header plus docs/manuals/en/ch01..ch10 in order; the committed file equals its output
- [ ] #3 A test fails when docs/MANUAL.md differs from the generated output (red on today's file, green after)
- [ ] #4 tests/test_manual_sat_commands.py and tests/test_sat_mqtt_state.py stay green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 review of the 387 MANUAL-only lines (126 headings; review file in the otgw-docs scratchpad, manual-only.md). FOLDED into the chapters, verified first: (1) 'Release artefact verification' -> ch02/h02 new section; checked on the real v2.0.0-beta.360 release: SHA256SUMS is an asset and sha256sum -c accepts it (ino.bin OK); added --ignore-missing (a partial download otherwise prints FAILED for absent files, shown); macOS shasum line checked with the Perl shasum only, not on a Mac. (2) BLE trust note -> ch05/h05 5.16, rewritten to the current code: 8-slot roster (SATtypes.h:509), auto-select of the only fresh sensor (SATble.ino:768-795), no connect or write to a sensor (no connect/createClient in SATble.ino).

DROPPED as stale against the code: 'Debugging the gate' (telnet key g: no case 'g' in handleDebug.ino; bMQTTGate is only set on prerelease builds, debugStuff.ino:119); BLE 'up to four sensors / four slots' (roster is 8, SATtypes.h:509); SAT 5.7 'Boot safety: CS=0 before the first cycle' (entry 129: no SAT command at boot); 'Auto-switch' wording of 5.6 (chapter 5.6 is current, ADR-249/TASK-2005); 7.9 'Exception (2)/(3)' rows (ESP8266 exception codes, not the S3); 6.9 'WebSocket live log (port 81)' (it is /ws on port 80, webSocketStuff.ino:39); 7.10 Factory Reset '/settings.json' (SETTINGS_FILE is /settings.ini, OTGW-firmware.h:138).

DROPPED as an older wording of a section the chapter keeps current (same heading in ch01-ch10, the chapter text is the maintained one): the remaining groups under Chapters 1, 3, 4, 5, 6, 7 (What Is New, Highlights, Navigation, Log Viewer, Temperature Graph, Network Status, Wi-Fi, FSexplorer, Theme, HA Overview/Discovery/Climate/SAT entities, 5.1-5.11, 6.1.2, 6.5-6.9, 7.1-7.8). 'Triggering a full rediscovery manually' is in ch04:241-250. 1.x migration: 'Going back to 1.x', the API v0/v1 note and the pin-number change are in ch02:20, :116-130, :276-279. TO ROBERT before any drop (coordinator condition 2): the module table 'NodeMCU for OTGW 1.x through 2.0, Wemos D1 mini for 2.3 and later' (MANUAL.md:163-168) is hardware history that the code cannot confirm and is in no chapter.
<!-- SECTION:NOTES:END -->
