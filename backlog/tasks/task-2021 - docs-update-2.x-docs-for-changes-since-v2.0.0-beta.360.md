---
id: TASK-2021
title: 'docs: update 2.x docs for changes since v2.0.0-beta.360'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T10:24:00Z'
updated_date: '2026-11-18T10:33:00Z'
labels:
  - docs
  - update-docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1036000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Standalone /update-docs-2x drift pass (coordinator, 2026-11-18), docs-only, one source per changed line; feeds the 20:00 beta notes (coordinate with 92). Phase 1: base v2.0.0-beta.360 (966546795), head 83648a4e9, 48 commits, 12 non-backlog. Areas: mqtt, otcore, sat, settings, ota, capture, main (7; skill rule says all chapters, this run maps per area: ch01/02/03/04/05/07/08 + MQTT.md + C4; ch06/ch09/ch10 have no changed source behind them). Unmapped: fsWriter.h/.ino (internal writer task, C4 only), settingsHelpGz.h (generated), scripts/README.md (is docs). Manual findings: OTGW32=0 in every chapter; ESP8266 mentions ch01 4, ch02 10, ch07 3, ch08 1, ch10 7, MANUAL.md 15 (Robert's call, not changed here). Commits: 8ffd2d121 TASK-2005, b88f267d7 TASK-2010/1804/1995, 6c7a7fdc8 ADR-252/234, 57fd0e30d + cc60bfd0e TASK-2007, 95cbb9ffb TASK-1991, 3bafc87b6 TASK-2012, 63e4cae0c TASK-1804, d6c2e0483 TASK-1995, e5fa0713c + fb18812f4 TASK-2013, 57d506ec7 TASK-2016. 370/371 added once landed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every user-visible change in 360..HEAD is in the manual chapter it belongs to (EN and NL), with the source behind it in the task notes
- [x] #2 docs/api/MQTT.md matches the MQTT changes (topics, payloads, migrations)
- [x] #3 C4 docs cover the new or changed internals (fsWriter, stale-override clear, settings load scope)
- [x] #4 The changes are handed to 92 for the 20:00 beta notes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 per change (otgw-docs): TASK-2005 margin 2.0/60 s: ch05/h05 already right (ch05:192, h05:217); docs/api/MQTT.md sat/pwm_switch_margin row was 1.5/30 s, fixed (SATtypes.h:541 default 2.0, SATcycles.ino:35 SAT_OVERSHOOT_SUSTAIN_MS 60000, settingStuff.ino:704-707 migration, :1182 clamp 0.5-5.0). TASK-2013: ch07/h07 and ch08/h08 telnet 'q' rows said settings.json, file is /settings.ini (OTGW-firmware.h:138); added that a preset is kept (settingStuff.ino SettingsLoadScope); C4 settings readSettings entry gets the load scope and the line moves 504 -> 581. TASK-1991: MQTT.md override section gets the after-connect sweep (OTGW-Core.ino staleOverrideClearStep, MQTTstuff.ino staleOverrideClearTick, OVR_CLEAR_GAP_MS 100); C4 mqtt IS_CONNECTED too. TASK-2010: MQTT.md thermal model section notes learning only while SAT controls (b88f267d7, _thermal_lastPassControlled). TASK-2007: ch02 already promises settings are kept, no manual change; C4 utilities gets a fsWriter section (fsWriter.h API, task 'fswriter' fsWriter.ino:96, OTA call sites OTGW-ModUpdateServer-esp32.h:663/668/786/816/885). TASK-1804: no manual lists OT-Direct error strings, parity with the PIC, no change. TASK-2012/2016: scripts/README.md updated in their own commits. Not documented on purpose: TASK-1995 [fs-slow]/[loop-stall] diagnostics (investigation open). Findings for later: ch04/h04 do not describe the gateway override sensors (ADR-118) at all; ESP8266 mention counts as in the description. No em dashes, no OTGW32 added.

2026-11-18 AC4: change list with sources sent to 92 (BlueFlame) for the 20:00 beta notes.

2026-11-18 92 confirms the 7 items match the 20:00 draft one to one; advised to keep TASK-1995 as a 'Diagnostics (no change in behaviour)' line ([fs-slow], helperStuff.ino:257, >= 100 ms). Task stays In Progress until 370/371 land and are added.
<!-- SECTION:NOTES:END -->
