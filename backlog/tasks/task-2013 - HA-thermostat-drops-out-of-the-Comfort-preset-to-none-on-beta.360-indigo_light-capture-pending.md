---
id: TASK-2013
title: >-
  HA thermostat drops out of the Comfort preset to none on beta.360
  (indigo_light), capture pending
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T06:24:00Z'
updated_date: '2026-11-18T07:51:00Z'
labels:
  - bug
  - needs-info
  - sat
dependencies: []
priority: high
ordinal: 1031000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by indigo_light in #beta-testing 2026-11-17 19:20 UTC (https://discord.com/channels/812969634638725140/914498730001072149/1572330567478018109): on beta.360 the HA thermostat leaves Comfort and goes to preset none (21 C) for no obvious reason, about 12 h into a 24 h Windows capture with a nightly reboot planned. Robert asked for a full capture; indigo will post the logs on 2026-11-18. Not TASK-1771 (fixed in beta.184). Not analysed: no capture yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The capture shows when and why the preset changed, cited
- [x] #2 The capture tool never sends 'q' (start or reconnect); its settings dump uses 'D' only; a host test is red on the old tool and green on the fix
- [ ] #3 A settings reload (readSettings at runtime, telnet q) keeps an active preset and its target; a manual target change still clears it; host test red on the old code, green on the fix
- [ ] #4 Bench (09): preset comfort, then telnet q: sat/preset stays comfort on the fix and drops to none on the old build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 HYPOTHESIS (comms 92, not verified, no capture of indigo yet): with satfollowthermostat on, a confirmed thermostat MsgID 16 change becomes the SAT target and also clears an active preset to none (manual ch05 5.21, 'A thermostat change also clears an active preset'; SATcontrol.ino:2497 active = satIsEnabled() && bFollowThermostat). If indigo's thermostat (or its program) changes its own setpoint at the moment the preset drops, that would explain it. Check in his capture: sat/follow_thermostat_enable, SATFOLLOW telnet lines and MsgID 16 T frames at the drop time. Found while analysing marceld91d's capture (20261117-081252), where follow is off and MsgID 16 went 19.0 -> 17.0 without effect.

2026-11-18 07:16 UTC ROOT CAUSE reported by indigo_light with log evidence (https://discord.com/channels/812969634638725140/914498730001072149/1572510726868832257), replaces the follow-thermostat hypothesis above: the capture tool sends 'q' over telnet at start and after every telnet reconnect (scripts/otgw_capture.py:1095 send_key_and_drain("q")); in the firmware 'q' is 'Read settings' -> readSettings(true) (handleDebug.ino:387-389, verified on origin/dev); re-applying settings.ini puts back the stale SATtargettemp=21.00 and clears the active preset to none. His evidence: handleDebugC Read settings + updateSettin field[SATtargettemp] newValue[21.00] at 09:24:51 (capture start), 09:54:15 and 10:02:11 (after 1 s telnet gaps), each matching MQTT sat/preset -> none and sat/climate_target -> 21.00; also at the start of his Mac captures on 16/11. Two issues he names: (1) selecting a preset or the HA card does not update SATtargettemp in settings.ini, (2) a settings re-read re-applies SATtargettemp and overrides an active preset although nothing changed. Workaround: save the target as 20.5. Capture: 20261117-092448-beta360-day-clean.zip (13.5 MB, msg 1572510559461576726), not downloaded (needs OK). Not yet reproduced by us.

2026-11-18 AC1 (otgw-docs): indigo_light day capture 20261117-092448 (beta.360, sha256 61d102d58effe099f83e0f235f1e4ba465a73cb86b84a87477c30eee94d2a6fb, #beta-testing msg 1572510559461576726; kept local in RvdB/OTGW-field-data/indigo_light/20261117-092448/, timeline in analysis/task2013-preset-drops.txt). Device clock runs about 1.95 s ahead of the host (same sat/preset publish: 09:31:53.820 device vs 09:31:51.875 host, 09:54:16.212 vs 09:54:14.255). Every preset drop: (1) capture start, device 09:24:51.314 handleDebug(388) Read settings, 09:24:51.398 updateSetting SATtargettemp=21.00, host 09:24:49.711 sat/preset=none; (2) gaps.log 09:54:12-13 telnet read failed then reconnect, device 09:54:15.919 Read settings, host 09:54:14.255 none; (3) gaps.log 10:02:08-09 read failed then reconnect, device 10:02:11.738 Read settings, host 10:02:10.088 none. Counts: 3 Read settings, 3 drops to none, 0 drops without a q before. Comfort was set again by the user at 09:31:51 and 09:55:19; 22:29:59 preset=sleep is a schedule, not a drop. The follow-thermostat hypothesis above does not apply: no drop without a preceding q.

2026-11-18 part 1, tools (otgw-docs): otgw_capture.py maybe_send_dump() sends only 'D' (dumpDebugInfo, handleDebug.ino:10, prints the settings itself); after_connect() re-arms the dump on every reconnect, which is why each reconnect also sent 'q'. scripts/README.md updated. Red/green: tests/test_capture_tool.py test_i (no q at start) and new test_i2 (no q after a reconnect). Old tool: AssertionError, keys received ['1','3','q','D','q','D','1','3']; fix: OK. Full capture suite: 148 OK (2 skipped).
<!-- SECTION:NOTES:END -->
