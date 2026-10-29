---
id: TASK-1482
title: >-
  SAT restart: stay quiet until a valid boiler reply and outside temperature,
  restore PWM enabled (Sergeant D entries 120/121)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T19:01:00Z'
updated_date: '2026-10-29T20:36:00Z'
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
- [ ] #1 Host test on the real boot/control path: after boot with no boiler reply or no valid outside temperature SAT queues no CS/CH/MM of its own (red on HEAD: CS=34.1-like setpoint with outside 0.0); once both are valid SAT takes over at the next tick
- [ ] #2 PWM enabled state persisted and restored across a reboot (red on HEAD: continuous after a restart in PWM); phase and timers start fresh
- [ ] #3 firmware-sat-spec.md and sat-comparison.md describe the restart sequence with entry 121 and the thermo-nova citations
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
<!-- SECTION:NOTES:END -->
