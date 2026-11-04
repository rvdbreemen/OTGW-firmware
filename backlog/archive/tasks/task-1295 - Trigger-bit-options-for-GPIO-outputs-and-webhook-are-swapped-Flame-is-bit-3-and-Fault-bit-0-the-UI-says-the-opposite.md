---
id: TASK-1295
title: >-
  Trigger-bit options for GPIO outputs and webhook are swapped: Flame is bit 3
  and Fault bit 0, the UI says the opposite
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T07:40:00Z'
updated_date: '2026-10-22T20:55:00Z'
labels:
  - webui
  - gpio
  - webhook
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 420000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1281 review (2026-10-22), pre-existing. OTcurrentSystemState.Statusflags = (MasterStatus<<8)|SlaveStatus (OTGW-Core.ino:2461); in the slave status byte bit 0 = fault and bit 3 = flame (OT spec MsgID 0 LB). outputs_ext.ino:85 and webhook.ino evalTriggerBit test (1U << iTriggerBit). ENUM_OPTS in data/v2.js (~2743) maps 0 to 'Flame' and 3 to 'Fault' for both gpiooutputstriggerbit and webhooktriggerbit, so a user who picks Flame gets a relay or webhook that follows the fault bit, and the other way round. Decide whether the labels or the bit numbers are wrong (check the 1.x classic UI and docs/api for the documented meaning before changing stored values), fix, and prove it with a replay that raises the flame bit and the fault bit separately.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The option labels match the bit the firmware tests; existing stored values keep their documented meaning or are migrated, with the choice recorded
- [x] #2 Bench or replay evidence: a frame with only the flame bit set triggers the output/webhook configured as Flame and not the one configured as Fault, and the reverse
- [x] #3 settings-help.json texts for gpiooutputstriggerbit and webhooktriggerbit name the bits correctly
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Decision on AC1: the stored value is the bit number of OTcurrentSystemState.Statusflags (outputs_ext.ino:85, webhook.ino:391); the 1.x classic UI offered it as a plain 'Trigger Bit (0-15)' (otgw-1.x.x index.js:5272, 5728), Webhooktypes.h:25 defaults to 1 with the comment 'CH mode', and OT MsgID 0 LB has bit 0 = fault, 3 = flame (spec lines 1443-1446). Only the v2 ENUM_OPTS labels (v2.js ~2767) were wrong, so the labels and help texts change and stored values stay; the CHANGELOG tells v2-alpha users who picked 'Flame' (stored 0) to reselect. Note: the 1.x ADR-022 lists bits 0-7 as master bits, which the code never implemented (it tests the slave byte at 0-7); recorded here, not changed. Changes: v2.js labels, settingsHelp.h texts with bit numbers, restAPI.ino gpiooutputstriggerbit max 16 -> 15 (matches constrain 0..15 and the webhook), tests/webui/trigger-bit-labels.test.mjs, CHANGELOG. Evidence: AC2 bench webhook: trigger bit 3 with the replay tests/webui/fixtures/status-bits-replay.txt (LB 00 -> 08 flame -> 00 -> 01 fault -> 00) fires ON/OFF only around the flame frames, trigger bit 0 only around the fault frames (PC listener + telnet); the GPIO output uses the same Statusflags test (outputs_ext.ino:85). AC1/AC3: before/after screenshots of the select and the help popover on the bench; the webui test fails on the old v2.js. Not on otgw-1.x.x (its UI has no labels).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 AC2 bench (OTGW32 .88.61, alpha.460 firmware, replay tests/webui/fixtures/status-bits-replay.txt: boiler status LB 00 -> 08 flame only -> 00 -> 01 fault only -> 00, 18 s each; webhook URLs to a listener on the PC, telnet capture in parallel): trigger bit 3: 'Webhook: bit changed -> ON' at 22:08:24.558, the first [---F----] frame decoded at 22:08:24.552; OFF at 22:08:42.6 when the flame frames end; nothing at the [E-------] fault frames (22:09:00). Trigger bit 0: nothing at the flame frames (22:10:12); ON at 22:10:48.445 on the first fault frame (decoded 22:10:48.436), OFF at 22:11:06.6. Listener log: GET /on, /off per run, HTTP 200. So bit 3 = flame, bit 0 = fault in the firmware; the GPIO output tests the same Statusflags expression (outputs_ext.ino:85). Evidence %TEMP%\bench-1295\telnet-bit3.log, telnet-bit0.log, hooks.log. Before screenshots on the old UI: select shows 0=Flame .. 3=Fault, help text without bit numbers (before-*.png). Webhook settings restored, fixture deleted.

2026-10-22 22:3x PAUSED for a laptop restart (maintainer request). State: all edits uncommitted in the dev working tree: v2.js ENUM_OPTS labels, settingsHelp.h texts (final wording 'Boiler status bit ... Other bits only via the REST API', JSON 18398 B of 18432), restAPI.ino max 16->15, outputs_ext.ino comment (Statusflags layout), docs/c4/c4-code-sensors.md bit list, CHANGELOG entry (symmetric migration note + REST range), tests/webui/trigger-bit-labels.test.mjs (labels, help, fixture decoder; env OTGW_V2_JS / OTGW_SETTINGS_HELP_H for old copies: 4 fail on HEAD, 33 OK on the fix with settings-help.test), tests/webui/fixtures/status-bits-replay.txt. Review (31 agents) confirmed only doc/test points, all fixed; follow-ups TASK-1304 (ADR-022 erratum) and TASK-1305 (1.x REST max). Bench: AC2 done (see above); fs+app of an intermediate build flashed, after-screenshots taken with the intermediate help text (8/9 clause). TO RESUME: build.bat, flash app via safeboot (help text is firmware), rerun triggershot.py (jobs tmp) for the final after-* screenshots, check AC1 + AC3, evaluate --quick, bump, commit (explicit paths incl. task files 1295/1304/1305), push origin dev, graphify update src, report in #alpha-testing, build zip from HEAD.

2026-10-22 22:55 resumed after restart: build.bat (combo) SUCCESS 22:47, app flashed USB COM4 --update --app (compiled 22:44:45, 178 settings keys unchanged before/after). Device serves settings-help.json (18398 B) with the new texts, /api/v2/settings gpiooutputstriggerbit max 15, v2.js labels 0=Fault..3=Flame (grep of the served file; the LittleFS on the bench is the intermediate image, labels identical). Final screenshots after-final-*.png: stored 0 shows 'Fault' (before: 'Flame'), help popover gives the bit numbers. Webui test on HEAD copies: 4 fail; on the fix: 33 pass (with settings-help.test). evaluate --quick: 0 fail, 0 warn.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Trigger-bit labels for the GPIO output and the webhook were swapped in the v2 settings page: stored value = bit number of OTcurrentSystemState.Statusflags (slave byte 0 = fault, 3 = flame, OT MsgID 0 LB); v2 offered 0 as Flame and 3 as Fault. Fixed the labels (v2.js ENUM_OPTS), the help texts with bit numbers (settingsHelp.h), the REST max for gpiooutputstriggerbit 16 -> 15 (matches the constrain), the outputs_ext.ino comment and c4-code-sensors bit list; stored values unchanged, CHANGELOG tells v2 users to reselect. Evidence: AC1 before/after screenshots (%TEMP%/bench-1295 before-*.png vs after-final-*.png: same stored 0 shows Flame before, Fault after) plus tests/webui/trigger-bit-labels.test.mjs (4 fail on HEAD, 33 pass on fix). AC2 bench replay tests/webui/fixtures/status-bits-replay.txt on OTGW32: trigger bit 3 fired only on the flame frames, bit 0 only on the fault frames (telnet-bit3.log, telnet-bit0.log, hooks.log). AC3 settings-help.json served by the device carries the new texts; test asserts them. Not verified: the final littlefs.bin was not flashed on the bench (served v2.js of the intermediate image has identical labels). Follow-ups TASK-1304 (ADR-022 erratum), TASK-1305 (1.x REST max).
<!-- SECTION:FINAL_SUMMARY:END -->
