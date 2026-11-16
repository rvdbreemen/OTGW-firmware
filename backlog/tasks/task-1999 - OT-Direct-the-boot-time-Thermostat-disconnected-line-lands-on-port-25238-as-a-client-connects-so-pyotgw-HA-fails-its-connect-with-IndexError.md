---
id: TASK-1999
title: >-
  OT-Direct: the boot-time 'Thermostat disconnected' line lands on port 25238 as
  a client connects, so pyotgw (HA) fails its connect with IndexError
status: Done
assignee:
  - '@claude'
created_date: '2026-11-16T10:48:00Z'
updated_date: '2026-11-16T19:33:00Z'
labels:
  - bug
  - ot-direct
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1021000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by bench tester 09 on 2026-11-16 (.61, OT-Direct board without a thermostat, 2.0.0-beta.341+3cb72c3, LegacyPort25238Enabled on; evidence 09 scratchpad pyotgw/). pyotgw 2.2.3 (the library behind HA's OpenTherm Gateway integration) connects to socket://<ip>:25238 right after /ReBoot; the connect fails in 2 of 5 tries (plus 1 of 1 earlier) with IndexError: list index out of range in pyotgw.py:778 process_statusfields_v4. Cause from its debug log: during its connect sequence (PR=A..W, PS=1, summary, PS=0) the bridge pushes the unsolicited 'Thermostat disconnected' (in the failure 19 s after the reboot, 3 ms after PS=0); pyotgw takes it as the PS answer and parses it as the summary. Source: OTDirect.ino checkThermostatTimeout() (origin/dev ~:3478-3497) reports the line once when no thermostat frame was ever seen, on its first run with millis() > 3000, about 19 s after boot; port 25238 starts accepting at 16.8-17.4 s (5 tries). A connect long after boot succeeds (1/1); a Classic with PIC 6.8 (.63, beta.347) succeeds too (an ESP reboot does not reset the PIC). The line itself is PIC parity (TASK-1850, gateway.asm TDisconnect); its timing is not. Fix options, not decided here: report the boot-time line before 25238 accepts clients; do not send it to the bridge at boot (the PIC reports it long before any client connects); send it only on a real connected-to-disconnected transition. Also check pyotgw upstream for robustness against unsolicited lines.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce: a pyotgw connect to 25238 right after /ReBoot, on an OT-Direct unit without a thermostat, fails (IndexError) on the current code in at least 1 of 5 tries
- [x] #2 Fix: the same reproduction succeeds 5/5 (or 10/10); 'Thermostat disconnected' and 'Thermostat connected' still reach the bridge on a real thermostat loss and return (TASK-1850 parity)
- [x] #3 Host test on the real checkThermostatTimeout() / bridge path: no unsolicited boot-time line while a client handshake can be in progress; red on the old code
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Option B (chosen with the advisor; the coordinator preferred C): the boot-time line of a thermostat that was never seen goes to processOT() only, not to port 25238; every real transition (connected -> disconnected, disconnected -> connected) goes to both. Why: the PIC prints 'Thermostat disconnected' tens of ms after its own reset (RAM cleared at reset, gateway.asm 6.6 :1175-1185, so NoThermostat = 0 and the first CheckThermostat on an open line runs Disconnect :1741-1773), long before any 25238 client can exist; the firmware event (handlePICStatusToken) still gets it. Option A (report before 25238 accepts) races the network start: the ~19 s comes from loopOTDirect() starting after the network, not from the 3000 ms constant. Option C (only real transitions) would drop the boot event and leave picLineState 0, so a thermostat plugged in later would get no 'Thermostat connected' line, which the PIC does print (TConnect). Not covered by any option: a real plug-in or unplug during a handshake still sends an unsolicited line, as on a PIC. Test: tests/test_otd_thermostat_lines.py (real checkThermostatTimeout() slice) 'never' expects bridge [] and processOT [DIS]; new case 'late' expects bridge [CON, DIS], processOT [DIS, CON, DIS].
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 (95) fix in OTDirect.ino checkThermostatTimeout(): picLine(text, toBridge); the never-seen boot line calls it with toBridge=false. tests/test_otd_thermostat_lines.py: FIX 6/6 OK (never: bridge none, processOT 'Thermostat disconnected'; late: bridge CON;DIS, processOT DIS;CON;DIS); OLD (OTGW_OTD_REV=HEAD = a10a5cdcd, the code before the fix) 2 FAIL (never: bridge 'Thermostat disconnected'; late: bridge DIS;CON;DIS). Also green: test_otd_gateway_alt_slots 12 (1 skip), test_otd_no_setback 4, test_otd_thermostat_loss_time 6, test/host/test_override_reply OK, evaluate --quick 0 failed. AC1/AC2 (pyotgw 2.2.3 connect right after /ReBoot, 5/5 or 10/10, plus a real loss/return on the bridge) are 09's bench on .61; 09's earlier 2/5 failures on beta.341 are the AC1 reproduction if 09 confirms. Open: pyotgw upstream robustness against unsolicited lines (not checked yet; report to Robert).

pyotgw robustness (local read-only copy other-projects/pyotgw-master/pyotgw/pyotgw.py:769-781, version of that snapshot, not checked against current upstream): process_statusfields_v4() splits the line and indexes status_fields[0..13] without checking the field count, so any unsolicited line taken as the PS answer raises IndexError. A length check there would make pyotgw robust against PIC lines too (a real thermostat loss during the handshake still sends one, on a PIC as well). Reported to Robert via the coordinator as a possible upstream issue for mvn23/pyotgw; not filed by us.

Bench 09 on .61 (OTGW32, OT-Direct, no thermostat), 2.0.0-beta.352+e18ccbf, LegacyPort25238Enabled on for the test, 2026-11-16 ~14:2x (09 scratchpad b352\): sha256 match, githash e18ccbf, settings 'changed: none', bootcount only by OTA and the planned reboots, no crash. Raw boot capture: OLD (351+597c163) telnet 'Thermostat disconnected' x1 and bridge 25238 x1; FIX telnet x1 (the processOT line stays), bridge 25238 0 thermostat lines. pyotgw 2.2.3 connect right after /ReBoot (port open 16.7-17.4 s, connect done 18.5-19.3 s): FIX 10/10 OK; on beta.341 this morning 3/5 OK, 2 IndexError at pyotgw.py:778 (+1/1 failed in the heap run). Real thermostat loss/return not benched (no thermostat on .61, no way to simulate): host test only. AC1/AC2 for 09 to tick.

2026-11-16: pyotgw upstream issue opened on Robert's 'Post' (coordinator): https://github.com/mvn23/pyotgw/issues/107 (draft by 95 unchanged, repro inline: issue_cmd PS=1 takes the next queued non-frame line as the status line, process_statusfields_v4 IndexError at pyotgw.py:778; control without the unsolicited line parses).

Bench 09 on .61, beta.356+4601510 (code of pushed cf29dd198), 25238 on for the test then off: telnet 'Thermostat disconnected' x1, bridge 25238 0 thermostat lines (880 B), same as 352. Pushed cf29dd198, posted 1571965973295009792; upstream mvn23/pyotgw#107. 09 ticks AC1/AC2.

AC1/AC2 ticked by 09 (2026-11-16). AC1: pyotgw 2.2.3 connect to 25238 right after /ReBoot on .61 (OT-Direct, no thermostat), old code 2.0.0-beta.341+3cb72c3: 2 of 5 failed with IndexError at pyotgw.py:778, plus 1 of 1 in the heap run. Raw bridge on 351+597c163: 'Thermostat disconnected' x1 on the bridge. AC2: fix 352+e18ccbf, same reproduction 10/10 OK (connect done 18.5-19.3 s); raw bridge 0 thermostat lines while telnet still shows the line; again on 356+4601510 (bridge 0). 'Thermostat connected/disconnected' on a REAL thermostat loss and return: HOST TEST ONLY, no thermostat on the bench (accepted by 95). Evidence: 09 scratchpad pyotgw/, b352/ (old351/, new352/, bc/), b356/raw/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT-Direct: the boot-time 'Thermostat disconnected' of a thermostat never seen goes to processOT() only, not to port 25238 (PIC prints it before any client can connect). AC1/AC2 (09 ticked): bench .61, pyotgw 2.2.3 connect after /ReBoot: old 341 3/5 (IndexError pyotgw.py:778), fix 352 10/10; 356 (= pushed cf29dd198): bridge 0 thermostat lines, telnet still shows the line. AC3: tests/test_otd_thermostat_lines.py 6/6 on the fix, 2 FAIL on the old code. Host-only: a real thermostat loss/return (no thermostat on the bench). Upstream: mvn23/pyotgw#107 (length check in process_statusfields_v4). Pushed cf29dd198, posted 1571965973295009792.
<!-- SECTION:FINAL_SUMMARY:END -->
