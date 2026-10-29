---
id: TASK-1406
title: >-
  PIC settings: reset_cause is a raw 0xFF byte after a PIC flash,
  /api/v2/pic/settings emits invalid UTF-8 JSON
status: Done
assignee:
  - '@claude'
created_date: '2026-10-26T06:05:00Z'
updated_date: '2026-10-29T06:18:00Z'
labels:
  - pic
  - bug
milestone: 2.0.0
dependencies: []
ordinal: 544000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench 2026-10-26 (Classic + S3 Mini .63, alpha.508, after a same-version gateway.hex 6.8 re-flash, TASK-1255): the PR= table showed Reset cause as the replacement character; GET /api/v2/pic/settings returned "reset_cause":"\xFF" (one raw byte), which is not valid UTF-8, so a strict JSON parser rejects the whole reply. Before the flash it read 'External reset'. The PR=Q answer after a PIC reprogram is stored and emitted without validation. Also seen: the post-flash settings poll ran 12 GETs, above PICSET_MAX_ITER=9 (check whether a field never arrives). Evidence: scratchpad task1255/ (screenshots, network list, timeline).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A non-printable or non-ASCII PR=Q reply is stored as unknown (or escaped), and /api/v2/pic/settings stays valid UTF-8 JSON (host test with a 0xFF answer, red on HEAD)
- [x] #2 The v2 PR= table shows a readable value for that case
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Port copyPicSettingValue() (header-only picSettingValue.h) from 1.x TASK-1430: printable answer unchanged, any other byte -> "0x" + hex.
2. handlePRresponse(): clean into a local buffer, compare and store the cleaned value (no republish of the same raw answer).
3. sResetCause 4 -> 8 bytes ("0xFF" needs 5).
4. Host harness slicing the REAL handlePRresponse() + PicSettingsSection per git revision, old-vs-fix.
5. v2 table: run the real picCode() from v2.js on the stored value.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28: 1.x sibling TASK-1430 (GH #688, field report on 1.7.8-beta.5) stores a non-printable PR= answer as "0x" + uppercase hex of every byte (0xFF -> "0xFF"), printable answers unchanged, via header-only copyPicSettingValue() (picSettingValue.h). Port the same representation so both lines publish the same value.

2026-10-29 (wt-1439, branch feat-1406-1439 on acab024d7):
- AC#1: python test/host/test_pic_settings_value.py --old-rev acab024d7 -> RESULT: PASS. OLD fails exactly D1 (PR: Q=0xFF published as \xFF), passes controls C1 (E unchanged), C2 (build date with spaces unchanged), C3 (repeat publishes nothing); FIX passes all 4. handlePRresponse() and PicSettingsSection are sliced by anchor from the revision under test; PICreturnSensor.h and picSettingValue.h are the real headers; only debug/MQTT/WebSocket are doubled.
- AC#2: the real picCode() and reset_cause case from data/v2.js run in node: "0xFF" -> "0xFF", "E" -> "External reset", raw 0xFF byte -> shown as the raw byte (old behaviour). /api/v2/pic/settings emits state.picSettings.sResetCause, which is now printable ASCII.
- build.bat (OTGW_BUILD_JOBS=4) esp32-combo alpha.524+acab024 with these changes: .ino.bin/.elf 06:42; evaluate.py --quick 79 passed 0 failed.
- Not reproduced on a gateway: needs a PIC answering PR=Q with 0xFF, which only appeared after a PIC reflash (not done: never test-flash the PIC).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A PR= answer that is not plain text is now stored and published as hex (0xFF -> "0xFF"), as on the 1.x line, so otgw-pic/settings/reset_cause and /api/v2/pic/settings stay valid UTF-8 after a PIC reprogram. Printable answers are unchanged; a repeated raw answer is not re-published (comparison on the cleaned value). sResetCause grew from 4 to 8 bytes.

Changes: picSettingValue.h (copyPicSettingValue), handlePRresponse() in OTGW-Core.ino, PICtypes.h, CHANGELOG, MQTT.md.

Tests: test_pic_settings_value.py on the real handlePRresponse(): old (acab024d7) fails only D1 (publishes \xFF), fix passes all 4; real v2.js picCode() shows "0xFF" for the stored value; build esp32-combo green; evaluate.py 79/0.

Not verified on a gateway: needs a PIC answering 0xFF, which only follows a PIC reflash (not done).

Builds: combo only (maintainer 2026-10-29): esp32-combo alpha.526 built and green. evaluate.py --quick 79 passed 0 failed after the rebase on b07e3194a; harness PASS old-vs-fix against b07e3194a.
<!-- SECTION:FINAL_SUMMARY:END -->
