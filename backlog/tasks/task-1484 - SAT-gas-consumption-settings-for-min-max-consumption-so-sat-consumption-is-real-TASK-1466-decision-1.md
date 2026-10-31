---
id: TASK-1484
title: >-
  SAT gas consumption: settings for min/max consumption so sat/consumption is
  real (TASK-1466 decision 1)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T20:29:00Z'
updated_date: '2026-10-31T05:21:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 643000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: wire up sat/consumption. Today minCons/maxCons are hard-coded 0 (SATcontrol.ino:3708-3720), so the topic is always 0. Add two persisted settings (min and max gas consumption in m3/h at min/max modulation), REST + v2 UI fields (Technical), settingsHelp, MQTT.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With min/max set, sat/consumption follows the modulation formula (host test red on HEAD: always 0); with 0/0 the topic is not published or stays 0 by design, documented; settings persist and load
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 close-out: merged on origin/dev as eb563686b (alpha.538). Batch build 2.0.0-alpha.542+c00c5f0 (esp32-combo from origin/dev c00c5f025, 2026-10-30 00:47, exit 0, fresh artefacts, Discord msg 1565512587494301767); integration check on the merged set: evaluate --quick 100%, 0 failed; every tests/test_sat_*.py plus tests/test_manual_sat_commands.py exit 0. Workflow journal: red on origin/dev a5efbf83d python tests/test_sat_consumption.py FAILED (failures=6 of 8: publish cases + settings round trip); red on the first lane commit 2c78a6752 failures=1 (a hot-water draw published a non-zero value); a mutation of the updateSetting field goes red; green Ran 8, OK; 35 SAT/settings harnesses rc=0; evaluate --quick 79/0/0. Fresh rerun this session on c00c5f025: Ran 8, OK. Gate: boiler CH-mode bit (isCentralHeatingActive, slave status bit 1, as thermo-nova's MQTT coordinator centralheating, coordinator/mqtt/opentherm.py:60-61) and flame; in simulation the simulated flame stands in. Open: Q69 (CH-mode bit vs master CH enable vs SAT active) is queued in docs/sat/open-questions.md, not yet asked. Deliberate deviations: no MsgID 17 availability gate (thermo-nova sensor.py:241-244); with 0/0 the topic is not published and, being non-retained, subscribers keep the last value (documented in MQTT.md). Persistence is host-tested with the real writeJsonFloatKV/parseJsonKVLine/clampSettingFloat; no on-device run and no v2 UI screenshot.

alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.

2026-10-31 close-out (session otgw-firmware-ea): the shared blocker is resolved. The alpha.542 FS-OTA failure is fixed in later builds; the OTA of alpha.564+d6dd798 to OTGW32 .61 confirmed /v2.js complete (261520 B = Content-Length) and 173 settings keys kept. Bench on alpha.564 (.61, SAT simulation): satminconsumption 0.5 / satmaxconsumption 2.0 set through REST; sat/consumption published on the broker (0.000, no flame); both values survived a reboot (REST and settings.ini SATminconsumption 0.50). Not checked: a non-zero value with a live flame (the bench has no boiler). Q69 was answered (entry 144) and continues in TASK-1506.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
sat/consumption is now real: two persisted settings satminconsumption / satmaxconsumption (m3/h at min/max modulation, 0..8, default 0, thermo-nova config_flow.py:756-762, const.py:163-164), REST GET/POST, v2 UI SAT Technical > Boiler, settingsHelp, MQTT.md, CHANGELOG, EN/NL manuals. Value = min + mod/100*(max-min) while boiler CH mode and flame are on, else 0 (thermo-nova sensor.py:252-259); not published unless both settings are above 0 (sensor.py:62-66). Merged eb563686b, alpha.538.

Evidence per AC:
- AC#1 formula: tests/test_sat_consumption.py compiles the real publish block; 0.3/2.5 at 40% gives 1.18, 0% and 100% give min and max; red on a5efbf83d (pub=0, value -1), green Ran 8 OK, rerun OK on c00c5f025 on 2026-10-30.
- AC#1 0/0: not published by design, covered by the test and documented in MQTT.md.
- AC#1 persist and load: real writeJsonFloatKV + parseJsonKVLine + clampSettingFloat round trip (0, 0.35, 2.5, 8; 9.5 clamps to 8), plus a regex check that each updateSetting line writes its own field (a mutation goes red).
- Build 2.0.0-alpha.542+c00c5f0 exit 0; evaluate 100%, 0 failed.
Not verified: on-device persistence across a reboot, the topic on a real broker, the v2 UI rendering. Open: Q69 (which CH signal) queued for Sergeant D, not asked; no MsgID 17 availability gate.
Status: In Progress. alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:FINAL_SUMMARY:END -->
