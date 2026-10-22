---
id: TASK-1294
title: >-
  LittleFS margin on 4MB boards is down to 8 KiB: gzip the staged web assets or
  move static text out of the image
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-22T07:32:00Z'
updated_date: '2026-10-22T17:36:00Z'
labels:
  - build
  - webui
  - littlefs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 419000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TASK-1281 (settings-help.json, 17.8 KB = 5 LittleFS blocks) build.bat reports 'LittleFS budget OK: 664 KiB assets + 96 KiB reserve <= 768 KiB partition' (was 640 KiB). The next web asset of any size fails the ADR-190 budget gate. The firmware already serves a .gz sibling with Content-Encoding (webSendFile(path, type, gzip) in webServerCompat.h) but build.py's staging only minifies .js/.css and never gzips. Options to weigh (research first, per the open-source rule): (a) gzip v2.js/v2-bundle.css/settings-help.json in staging and serve the .gz sibling (v2.js 243 KB minified would drop to ~60 KB), checking the file-serve gate, ETag/cache headers (ADR-139) and the FSexplorer download path; (b) ship rarely used text (help JSON, otgw_simulation.log 12.7 KB) in the firmware image instead of LittleFS; (c) drop dev fixtures from the shipped image. Needs an ADR amendment to ADR-190 if (a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision recorded (ADR amendment if the staging pipeline changes) with the measured sizes per option
- [ ] #2 build.bat budget line shows at least 64 KiB headroom on esp32-combo after the change, with the before/after lines quoted
- [ ] #3 Bench: the v2 UI loads fully from the new image in Chromium with no 503/decoding errors; FSexplorer download of a gzipped asset still yields the original file
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) settings-help.json becomes src/OTGW-firmware/settingsHelp.h: static const char SETTINGS_HELP_JSON[] PROGMEM = R"json( ... )json"; (the header is the source of truth; data/settings-help.json removed). FSexplorer.ino: the /settings-help.json route calls a new serveProgmemAsset(path, mime, PGM_P body) next to serveVersionedAsset: ETag "<firmware githash>-<path>" (same shape as the FS-hash ETag), Cache-Control no-cache, 304 on If-None-Match, body via webSendPStream (no heap copy). 2) otgw_simulation.log becomes src/OTGW-firmware/otgwSimulationFixture.h (PROGMEM string, LF separated, verbatim content of today's file); OTGW-Core.ino replay: when /otgw_simulation.log is NOT on LittleFS, read lines from the PROGMEM fixture (cursor + wrap) instead of 'simulation disabled'; an uploaded LittleFS file keeps precedence (checked at each (re)open). /api/v2/simulate JSON: available always true; add "source": "file" | "builtin". data/otgw_simulation.log removed. 3) Tests: tests/webui/settings-help.test.mjs reads the JSON out of the header (size cap stays 18 KB; asserts data/settings-help.json is gone); a python test asserts the fixture header equals a canonical line list and that the replay reader handles both sources (source-level); test_static_integrity/test_build_staging adjusted if they list the files. 4) Docs: ch03/h03 sentence about the help file, docs/api openapi simulate schema (source field), CHANGELOG Changed. ADR-190 narrowing recorded in a follow-up task (adr-kit unavailable this session). 5) Evidence: build budget line before (664 KiB) vs after; bench: fw + fs update, /settings-help.json 200 with firmware-hash ETag and 304, help popover loads in the browser, /simulate/start with no file replays the built-in fixture (TrSet/T frames on MQTT), an uploaded fixture takes precedence, then removed again. No SAT behaviour change.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 cycle pickup: option (a) gzip conflicts with a standing maintainer directive recorded twice: ADR-190 status line ('He rejected gzip and asked for a minified image'), ADR-139 D ('no gzip, no .gz siblings, maintainer directive; TASK-433 stays disabled') and the build.py comment at ~944. Not pursued without an explicit reversal by the maintainer. Measuring the other levers (fonts, fixture, PROGMEM move) to put a concrete choice to him.

2026-10-22 maintainer decision (Robert, in session, AskUserQuestion): only 'move settings-help.json and otgw_simulation.log into firmware flash'. Not chosen: dropping the shipped PIC hex files (116 KB, downloaded on demand from otgw.tclcode.com at OTGW-Core.ino:6082), dropping the self-hosted fonts (69 KB), reversing the no-gzip directive. Scope: serve /settings-help.json from PROGMEM (firmware githash ETag, 304 on If-None-Match), build the replay fixture into the firmware as the fallback when no /otgw_simulation.log is on LittleFS (an uploaded file keeps precedence, the bench depends on it), remove both from data/, expected image 664 -> ~632 KiB.

2026-10-22 BEFORE: build-458.log 'LittleFS budget OK: 664 KiB assets + 96 KiB reserve <= 768 KiB partition'; staged content 604212 B incl. settings-help.json 18300 B and otgw_simulation.log 12716 B (both 4 KiB-block files: 5 + 4 blocks = 36 KiB of image); /settings-help.json answered from LittleFS with ETag "4ffa66d-/settings-help.json" (fs hash) on the bench; /api/v2/simulate reported file /otgw_simulation.log, available true only because the file ships in the image. App flash 89.3% (2049071 of 2293760 B). ADR-190 amendment deferred to TASK-1303 (adr-kit unreachable).
<!-- SECTION:NOTES:END -->
