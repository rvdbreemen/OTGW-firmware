---
id: TASK-1482
title: >-
  SAT restart: stay quiet until a valid boiler reply and outside temperature,
  restore PWM enabled (Sergeant D entries 120/121)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T19:01:00Z'
updated_date: '2026-10-30T00:00:00Z'
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
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT stays quiet after a restart until it has a valid boiler reply and a valid outside temperature (Sergeant D entries 120/121, 126, 129): with a valid restored state (PID + outside temperature < 1 h) it sends nothing at boot and takes over at its first pass; otherwise one hand-back CS=0/CH=1/MM=T and silence. The PWM-enabled flag is persisted and restored; phase and timers start fresh (thermo-nova pwm.py:111-114 via heating_control.py:181-184). Merged f07db1ec5, alpha.540, with TASK-1390.

Evidence per AC:
- AC#1: tests/test_sat_restart_quiet.py (17 tests, real SATcontrol.ino code compiled) red on f07db1ec5^ (failures=13, errors=1: e.g. CS=40.0 at t=31 s instead of the hand-back, no wait for a boiler reply), green 17/17 on c00c5f025 (rerun 2026-10-30).
- AC#2: test_pwm_enabled_survives_a_restart red (MODE 1) / green (MODE 2); test_no_pwm_file_boots_continuous green. The test asserts the restored mode only; "phase and timers start fresh" is by code reading (initSAT resets the phase), not asserted.
- AC#3: firmware-sat-spec.md and sat-comparison.md updated in f07db1ec5 with entries 121/126/129 and thermo-nova citations (climate.py:544-545, heating_control.py:181-191, pwm.py:111-114).
- Build 2.0.0-alpha.542+c00c5f0 exit 0; evaluate 100%, 0 failed.
Not verified: a real reboot on the bench. Open: Q66-Q68 (asked 2026-10-30, unanswered); the implementation follows our advice A.
Status: In Progress. alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:FINAL_SUMMARY:END -->
