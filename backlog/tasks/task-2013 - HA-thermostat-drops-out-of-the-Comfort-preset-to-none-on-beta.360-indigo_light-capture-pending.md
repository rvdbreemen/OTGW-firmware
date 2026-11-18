---
id: TASK-2013
title: >-
  HA thermostat drops out of the Comfort preset to none on beta.360
  (indigo_light), capture pending
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T06:24:00Z'
updated_date: '2026-11-18T09:27:00Z'
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
- [x] #3 A settings reload (readSettings at runtime, telnet q) keeps an active preset and its target; a manual target change still clears it; host test red on the old code, green on the fix
- [x] #4 Bench (09): preset comfort, then telnet q: sat/preset stays comfort on the fix and drops to none on the old build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 HYPOTHESIS (comms 92, not verified, no capture of indigo yet): with satfollowthermostat on, a confirmed thermostat MsgID 16 change becomes the SAT target and also clears an active preset to none (manual ch05 5.21, 'A thermostat change also clears an active preset'; SATcontrol.ino:2497 active = satIsEnabled() && bFollowThermostat). If indigo's thermostat (or its program) changes its own setpoint at the moment the preset drops, that would explain it. Check in his capture: sat/follow_thermostat_enable, SATFOLLOW telnet lines and MsgID 16 T frames at the drop time. Found while analysing marceld91d's capture (20261117-081252), where follow is off and MsgID 16 went 19.0 -> 17.0 without effect.

2026-11-18 07:16 UTC ROOT CAUSE reported by indigo_light with log evidence (https://discord.com/channels/812969634638725140/914498730001072149/1572510726868832257), replaces the follow-thermostat hypothesis above: the capture tool sends 'q' over telnet at start and after every telnet reconnect (scripts/otgw_capture.py:1095 send_key_and_drain("q")); in the firmware 'q' is 'Read settings' -> readSettings(true) (handleDebug.ino:387-389, verified on origin/dev); re-applying settings.ini puts back the stale SATtargettemp=21.00 and clears the active preset to none. His evidence: handleDebugC Read settings + updateSettin field[SATtargettemp] newValue[21.00] at 09:24:51 (capture start), 09:54:15 and 10:02:11 (after 1 s telnet gaps), each matching MQTT sat/preset -> none and sat/climate_target -> 21.00; also at the start of his Mac captures on 16/11. Two issues he names: (1) selecting a preset or the HA card does not update SATtargettemp in settings.ini, (2) a settings re-read re-applies SATtargettemp and overrides an active preset although nothing changed. Workaround: save the target as 20.5. Capture: 20261117-092448-beta360-day-clean.zip (13.5 MB, msg 1572510559461576726), not downloaded (needs OK). Not yet reproduced by us.

2026-11-18 AC1 (otgw-docs): indigo_light day capture 20261117-092448 (beta.360, sha256 61d102d58effe099f83e0f235f1e4ba465a73cb86b84a87477c30eee94d2a6fb, #beta-testing msg 1572510559461576726; kept local in RvdB/OTGW-field-data/indigo_light/20261117-092448/, timeline in analysis/task2013-preset-drops.txt). Device clock runs about 1.95 s ahead of the host (same sat/preset publish: 09:31:53.820 device vs 09:31:51.875 host, 09:54:16.212 vs 09:54:14.255). Every preset drop: (1) capture start, device 09:24:51.314 handleDebug(388) Read settings, 09:24:51.398 updateSetting SATtargettemp=21.00, host 09:24:49.711 sat/preset=none; (2) gaps.log 09:54:12-13 telnet read failed then reconnect, device 09:54:15.919 Read settings, host 09:54:14.255 none; (3) gaps.log 10:02:08-09 read failed then reconnect, device 10:02:11.738 Read settings, host 10:02:10.088 none. Counts: 3 Read settings, 3 drops to none, 0 drops without a q before. Comfort was set again by the user at 09:31:51 and 09:55:19; 22:29:59 preset=sleep is a schedule, not a drop. The follow-thermostat hypothesis above does not apply: no drop without a preceding q.

2026-11-18 part 1, tools (otgw-docs): otgw_capture.py maybe_send_dump() sends only 'D' (dumpDebugInfo, handleDebug.ino:10, prints the settings itself); after_connect() re-arms the dump on every reconnect, which is why each reconnect also sent 'q'. scripts/README.md updated. Red/green: tests/test_capture_tool.py test_i (no q at start) and new test_i2 (no q after a reconnect). Old tool: AssertionError, keys received ['1','3','q','D','q','D','1','3']; fix: OK. Full capture suite: 148 OK (2 skipped).

2026-11-18 part 2, firmware (otgw-docs): cause: with a preset active settings.sat.fTargetTemp holds the preset temperature while settings.ini keeps satUserTargetTemp() (settingStuff.ino writer, TASK-1487); readSettings() feeds every line through updateSetting(), so SATtargettemp=21 looked like a user change and ran satPresetClearForUserTarget() and satResetControlState(). Runtime callers of readSettings(): only telnet 'q' (handleDebug.ino:389); at boot the preset is restored afterwards by satLoadPresetState(). Fix: readSettings() opens a SettingsLoadScope (RAII, restores the previous value on every exit, the recursive call included); while it is open and a preset or window switch holds the target, the SATtargettemp branch leaves target and preset alone (if-branch, no early return, so the no-op check and dirty mark at the end of updateSetting() still run). thermo-nova restores preset, pre-custom and target after a restart (climate.py:176-182, :187-194) and clears a preset only on a manual temperature (:567-568); a settings reload has no counterpart there, so no SAT behaviour changes and no Sergeant D entry is touched. Claim: a reload no longer drops a preset; other setters keep their user-change side effects on a reload. Evidence: tests/test_sat_preset_persist.py 4 new cases, 13/13 OK; on origin/dev settingStuff.ino FAILED ('0 != 3: a settings reload dropped the active preset', 'readSettings() does not open the load scope first'). Labels: the reload in the test is a test-side copy of what readSettings() does (scope plus updateSetting()), not the real file loop; test_manual_target_after_a_reload passes on the old code too (no-regression). tests/test_sat_pid_resets.py harness got the flag stub (17/17 OK). evaluate.py --quick 0 FAIL; run_tests --changed 58/59 before the stub, the one failure was that harness compile.

2026-11-18 bench by 09 (otgw-bench-tester) on .61, reported to otgw-docs (AC4 is 09's to tick): first 368 run VOID (no boiler: SAT handed back 56 ms before 'q', which can clear a preset by itself); both counted runs with GW=L (transient loopback boiler, no setting, no satsimulation). Steps: sat/enabled=1; sat/preset=comfort (SATpresetcomfort 21, file SATtargettemp 22); telnet 'q'; sat/target=22. OLD 368+c0044a3: 10:07:08.304 Read settings, field[SATtargettemp] newValue[22.00] without no-op skip, same second sat/preset -> none and climate_target 21 -> 22 (RED). FIX 369+5897780 (appelfsha256 equals the .elf sha256, settings 'changed: none'): 10:13:53.669 Read settings, field[SATtargettemp] newValue[22.00] + '[Settings] no-op skip', sat/preset stays comfort and climate_target 21 for 15 s; then sat/target=22 -> preset none, climate_target 22.00 (GREEN, manual change still clears). Settings restored (165 keys, settings.ini identical to the start). 09's evidence: its scratchpad t2013\ (old368L-*, fix369A-*, fix369L-*, flash369.log, S-*.ini).

2026-11-18 published: rebased on 460199594 and rebuilt, 2.0.0-beta.369+fb18812 (app recompiled, the .ino.bin carries fb18812; safeboot unchanged, it has no githash); pushed 460199594..fb18812f4; #alpha-testing post message 1572542159293513769 (also names TASK-2016 and TASK-2012). The bench ran on 5897780, the same source before the docs-only rebase.

AC4 bench by 09 (2026-11-18, .61 OT-Direct, GW=L loopback boiler, satsimulation off; same steps on both builds: sat/enabled=1, sat/preset=comfort (SATpresetcomfort 21, file SATtargettemp 22), telnet 'q', sat/target=22). OLD 2.0.0-beta.368+c0044a3, 10:06-10:07: comfort -> climate_target 21, sat/target 21; 10:07:08.304 'Read settings', reload 'field[SATtargettemp], newValue[22.00]' with NO no-op skip; same second sat/preset -> none, climate_target 22 (RED). FIX 2.0.0-beta.369+5897780 (githash 5897780, appelfsha256 05150779... = .elf; full OTA, changed none), 10:12-10:14, SAT active (sat/outdoor_temp=8 injected non-retained; without it SAT stayed inactive after the OTA reboot, that run fix369L was also green): comfort -> climate_target 21, sat/target 21; 10:13:53.669 'Read settings', 'newValue[22.00]' + '[Settings] no-op skip' (target left alone); sat/preset stays comfort, climate_target stays 21 (GREEN); 10:14:08 sat/target=22 -> sat/preset none, climate_target 22. A first 368 run without loopback is void: SAT handed the boiler back 56 ms before 'q'. Restored: settings as before the test (diff none, settings.ini identical), GW=2 master. Evidence: 09 scratchpad t2013/ (old368L-*, fix369A-*, fix369L-*).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: indigo_light's beta.360 day capture 20261117-092448 shows 3 drops to preset none, each right after the capture tool's 'q' (device 09:24:51.314 start, 09:54:15.919 and 10:02:11.738 after 1 s telnet gaps; device clock about 1.95 s ahead of the host), 0 drops without a preceding 'q'. AC2: otgw_capture.py sends only 'D'; test_i/test_i2 red on the old tool (keys ['1','3','q','D','q','D','1','3']), green on the fix; capture suite 148 OK (commit e5fa0713c). AC3: readSettings() opens a SettingsLoadScope and the SATtargettemp branch leaves a held preset alone while loading; test_sat_preset_persist 13/13, red on the old settingStuff ('0 != 3: a settings reload dropped the active preset'); the reload in the test is a test-side copy of readSettings(), the manual-change case is no-regression (commit fb18812f4, 2.0.0-beta.369). AC4: 09 on .61 with GW=L: 368 drops the preset on 'q', 369+5897780 keeps it (no-op skip) and a manual target 22 still clears it (ticked by 09, 83648a4e9). Published as 2.0.0-beta.369+fb18812, #alpha-testing message 1572542159293513769. Not covered: other setters still run their user-change side effects on a reload ('q' is a debug key).
<!-- SECTION:FINAL_SUMMARY:END -->
