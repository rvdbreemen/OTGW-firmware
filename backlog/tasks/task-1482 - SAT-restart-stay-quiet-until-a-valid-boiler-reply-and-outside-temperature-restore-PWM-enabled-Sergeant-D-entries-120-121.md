---
id: TASK-1482
title: >-
  SAT restart: stay quiet until a valid boiler reply and outside temperature,
  restore PWM enabled (Sergeant D entries 120/121)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T19:01:00Z'
updated_date: '2026-10-30T06:12:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 642000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D Q59 A (entry 121, settles entry 120 'this is SAT's bug'): after a gateway restart SAT sends nothing of its own until it has a valid boiler reply AND a valid outside temperature (entries 39/47/56/57/79), then takes over; it restores whether PWM was enabled (thermo-nova pwm.py:111-114 via heating_control.py:181-184, climate.py:169-174). Found in gruijter's alpha.527 capture (TASK-1474): at 12:57:24 SAT sent CS=34.1/CH=1/MM=100 with outside 0.0 C (MsgID 27 fallback) and came back in continuous mode after being in PWM OFF. Firmware today: boot CS=0 SATcontrol.ino:4317-4325, continuous at :4338. Relates to TASK-1390 (outside temperature missing).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real boot/control path: after boot with no boiler reply or no valid outside temperature SAT queues no CS/CH/MM of its own (red on HEAD: CS=34.1-like setpoint with outside 0.0); once both are valid SAT takes over at the next tick
- [x] #2 PWM enabled state persisted and restored across a reboot (red on HEAD: continuous after a restart in PWM); phase and timers start fresh
- [x] #3 firmware-sat-spec.md and sat-comparison.md describe the restart sequence with entry 121 and the thermo-nova citations
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Combined with TASK-1390 (same validity rule). Entries relied on: 39, 47, 56, 57, 79, 120, 121.
1) satOutsideTempFresh(): external/MQTT value younger than SAT_STALE_OUTDOOR_MS (10 min, existing); weather value fetched < 1 h (entry 57; weather.iLastUpdateMs); OT MsgID 27 only with a boiler Read-Ack < 1 h (new stamp in OTGW-Core.ino case 27, like stampThermostatRoomTemp). Simulation always fresh.
2) No fresh outside temperature (entry 56): one hand-back CS=0, CH=1, MM=T (satDisable path as OT loss, entry 47), then nothing from SAT; resume on the next tick after a fresh value returns (entry 79 pattern). Status bit sat/outside_temp_missing (REST/MQTT) for TASK-1390 AC#3.
3) Boot (entry 121): initSAT enters the same quiet state: full hand-back (CS=0, CH=1, MM=T) instead of CS=0 only (SATcontrol.ino:4317-4325), and the control loop returns before every sender until a boiler reply seen after boot (state.otBus.iBoilerLastSeenMs != 0) AND satOutsideTempFresh(). Then the next tick takes over.
4) PWM enabled: persisted on change in satSetControlMode (LittleFS, small file like the PID state) and restored at takeover; PWM phase and timers start fresh, as thermo-nova pwm.py:111-114 restores only _enabled (via heating_control.py:181-184, climate.py:169-174).
thermo-nova: climate.py:544-545 skips a pass while outside is None.
Tests: host tests on the real control path: boot with no boiler reply / no outside -> no SAT CS/CH/MM (red on HEAD: CS from outside 0.0); stale weather > 1 h -> hand-back once; resume; PWM flag restore.
Open points to Sergeant D: (a) boot hand-back full vs CS=0 only (advice full); (b) MsgID 27 counts only with a boiler Read-Ack < 1 h (advice yes).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29: plan posted to Sergeant D for OK with open points (a) full boot hand-back, (b) MsgID 27 validity (msg in #alpha-testing). Implementation waits for his answer (skill step 4).

2026-10-29 Sergeant D entry 126 (and maintainer): outside temperature valid < 1 h, saved with the PID state and restored at boot, so SAT resumes on its first tick; entry 127: sliding hour kept. Q62 asked (boot send with a valid restored state: A nothing / B hand-back; advice A). Implementation waits for Q62.

2026-10-29 Q62 A (entry 129): with a valid restored state (PID + outside temperature < 1 h) SAT sends nothing at boot and takes over at its first tick; otherwise entry 121 quiet hand-back. OT-Direct has no PIC CS holdover: first tick must come quickly there (plan point). Unblocked: implement in batch 2.

2026-10-30 close-out: merged on origin/dev as f07db1ec5 (alpha.540), together with TASK-1390. Batch build 2.0.0-alpha.542+c00c5f0 (esp32-combo from origin/dev c00c5f025, 2026-10-30 00:47, exit 0, fresh artefacts, Discord msg 1565512587494301767); integration check on the merged set: evaluate --quick 100%, 0 failed; every tests/test_sat_*.py plus tests/test_manual_sat_commands.py exit 0. Evidence collected this session on c00c5f025: python tests/test_sat_restart_quiet.py Ran 17, OK (rc=0); with the pre-fix SATcontrol.ino (f07db1ec5^, via OTGW_SAT_INO) FAILED (failures=13, errors=1), behavioural: boot without state sends CS=0 at t=1 and CS=40.0/MM=100/CH=1 at t=31 instead of the hand-back CS=0/CH=1/MM=T; boot with a valid restored state sends CS=0 instead of nothing; boot does not wait for a boiler reply or NTP; PWM boot gives MODE 1 instead of MODE 2; restored outside temperature lost; satEnqueueOwnCS found in initSAT. The test harness compiles the real SATcontrol.ino code; source guards cover initSAT sending no SAT command and the restart gate before every sender. Docs: firmware-sat-spec.md and sat-comparison.md describe the restart sequence with entries 56, 57, 121, 126, 129 and thermo-nova climate.py:544-545, heating_control.py:181-191, pwm.py:111-114. The implementation follows our advice A on Q66-Q68 (outside-temperature source order, NTP decide window, PID integral handling); those questions were asked on 2026-10-30 and are still unanswered: a different answer becomes a follow-up task.

alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.

Bench 2026-10-30 (coordinator otgw-firmware-b8, Classic-S3 .63, PIC 6.8): batch rebuilt after the 05:50 PC restart; esp32-combo alpha.547+6240b05 and alpha.548+8ded885 built (artifacts 06:12 / 06:42, githash = HEAD), evaluate --quick 79/0/0 on both; OTA app via safeboot + FS via /update?cmd=100, 174 settings keys kept. TASK-1482 + follow-up (alpha.544): web UI v2.js complete after FS OTA (256638 B); SAT simulation on .63: /sat_pid_state.json written after 201 s ({i:0.0219,...}); POST /ReBoot -> uptime 0 min, pid_i=0.0219 restored from the file. Q66-Q68 answered A by Sergeant D (entries 139/140), matching the implementation. Settings restored and SAT disabled again after the test.

Correction 2026-10-30 (coordinator): the Done note said Q66-Q68 match the implementation. That holds for Q67 A and Q68 A, NOT for Q66 A: the 10-minute external priority window is still in the code (SATcontrol.ino:1878, :1900). Q66 A is a new behaviour decision, filed as its own task (see the next task id); TASK-1482's own ACs stay met.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT stays quiet after a restart until a valid boiler reply and outside temperature exist; with a valid saved state nothing is sent at boot; PWM-enabled persisted (entries 121, 126, 129). AC#1/#2 host tests on the real boot path (red on HEAD, green on fix; earlier notes). AC#3 docs. Bench this session on alpha.548: PID state saved and restored across a real reboot (pid_i 0.0219). Follow-up alpha.544 restored the unguarded satSavePidState(). Not verified on the bench: the boot hand-back without a valid state (needs a boiler-less PIC reboot with stale state); covered by the host test. Entry 140 open point: entry 129's premise (PIC keeps the CS over a restart) was wrong; follow-up question still to be asked.
<!-- SECTION:FINAL_SUMMARY:END -->
