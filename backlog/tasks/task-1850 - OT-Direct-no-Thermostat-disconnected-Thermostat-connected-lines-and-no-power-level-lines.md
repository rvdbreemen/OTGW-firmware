---
id: TASK-1850
title: >-
  OT-Direct: no "Thermostat disconnected"/"Thermostat connected" lines like the
  PIC
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T00:05:00Z'
updated_date: '2026-11-19T22:44:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 929000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N16), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC prints these on the serial line (G:1761-1764, :1776-1778, :5490-5491; tests/standalone-1.stc, standalone-2.stc, standalone-7.stc).
- OT-Direct sends only a WebSocket status and a debug line (O:2807-2843). The firmware's event table knows the PIC tokens (OTGW-Core.ino:5528-5532) but never gets them on OT-Direct.
- Scope: the thermostat lines only. The power-level lines (G:1532-1551) moved to TASK-1945 (research first).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 on thermostat loss and return the PIC lines go to port 25238 and processOT
- [x] #2 host test on the real checkThermostatTimeout
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PIC 6.8 semantics, verified in gateway.asm (TDisconnect/TConnect strings :5490-5491, Disconnect :1776-1778, reconnect :1755-1764; onoffflags has no explicit init, so at boot the thermostat counts as connected) and the PIC's own tests: standalone-2.stc (lost -> 'Thermostat disconnected', back -> 'Thermostat connected'), standalone-1.stc / standalone-7.stc (no thermostat at boot -> 'Thermostat disconnected'). Current OT-Direct (OTDirect.ino checkThermostatTimeout): only a WebSocket status and a debug line; the reconnect is reported only when a setback was engaged; nothing when no thermostat is ever seen. Change, in checkThermostatTimeout() only: a small report state (unknown at boot, connected, disconnected); emit the exact PIC line through otDirectBridgeWriteLine() + processOT() (the same path as emitSummaryLine) on: connected/unknown -> disconnected after the existing timeout (iSetbackTimeout; for a thermostat never seen, counted from boot), and disconnected -> connected on the next thermostat frame, independent of the setback. Boot with a thermostat present: no line (as the PIC). Setback logic, WebSocket messages and the never-seen early return of the setback path stay as they are. Host test on the real checkThermostatTimeout() (extracted, stubs for the I/O) red on the old code, green on the fix: loss, return, return without setback, never seen at boot, present at boot. No build now (coordinator: memory); OTDirect.ino edits stay minimal because 95's 1848/1849 land first.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): timing depends on TASK-1728 and TASK-1802.

Part of the PIC 6.8 parity audit, parent TASK-1851.

Implemented (local, no build yet: coordinator memory freeze): checkThermostatTimeout() (OTDirect.ino) prints the PIC's 'Thermostat disconnected' / 'Thermostat connected' through otDirectBridgeWriteLine() + processOT() with a local report state (boot counts as connected). Setback logic and WebSocket messages unchanged. tests/test_otd_thermostat_lines.py compiles the real function: FIX 5/5 (present at boot: no line; loss: disconnected once; return without and with setback: disconnected, connected; never seen since boot: disconnected once after the timeout), on both port 25238 and processOT(); OLD (origin/dev) 4 of 5 red (present at boot green, as on the PIC). test_otd_gateway_alt_slots 12 PASS, static_integrity, ino_prototype_hoisting PASS; evaluate 85/0. AC2 ticked. AC1 waits for the build and a bench (thermostat unplug and replug on an OT-Direct unit, lines on port 25238 and as OTGW events). Not compiled on the ESP32 yet (strlcpy_P, PGM_P lambda).

Scope trimmed to the ACs (coordinator, 2026-11-14): title and description no longer promise the power-level lines; those are TASK-1945 (research first, consistent with TASK-1848's fixed 'Low power' answer to PR=P). AC1 bench reach: OT-Direct has no thermostat simulation (loopback simulates the boiler; otThermostatSeen is set only by real thermostat frames, OTDirect.ino:1301/2457), so loss and return need a physical unplug/replug: FIELD/MAINTAINER. Bench-testable without hands: the 'no thermostat since boot' case (one 'Thermostat disconnected' on port 25238 after iSetbackTimeout), on a unit without a thermostat (asked 09 whether .61 has one).

2026-11-19 triage (otgw-dev): AC1 hardware-blocked: OT-Direct has no thermostat simulation (otThermostatSeen is set only by real thermostat frames), so thermostat loss and return need a real thermostat on the OT-Direct bus of a bench unit.

2026-11-19 Robert 'A' (DM 1573103087613124832, reply to the coordinator's question on TASK-1847/1850; relayed by the coordinator): AC1 closed on host and bench evidence with a field waiver, because OT-Direct has no thermostat simulation (otThermostatSeen only from real thermostat frames, OTDirect.ino:1301/2457). Field check with a real thermostat moved to 2.1.0 follow-up TASK-2935. Fresh evidence this session: python tests/test_otd_thermostat_lines.py on origin/dev 27b5251de: Ran 6 tests, OK, rc=0 (present at boot: no line; loss: disconnected once; return with and without setback: connected; never seen: processOT only, not port 25238 (TASK-1999); plugged in later then lost: bridge connected, disconnected). Bench: the boot-time path on .61 (no thermostat) verified under TASK-1999 by 09 on beta.356. NOT verified: a real unplug/replug on hardware (TASK-2935).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT-Direct checkThermostatTimeout() prints the PIC 6.8 'Thermostat disconnected'/'Thermostat connected' lines on port 25238 and through processOT() (27309cb13; boot-time line to processOT only since TASK-1999, 9b2a23287). AC2: tests/test_otd_thermostat_lines.py on the real function, red 4/5 on the old code, green 6/6 on origin/dev 27b5251de (rc=0). AC1: closed with a field waiver by Robert 'A' (DM 1573103087613124832): no thermostat simulation on OT-Direct; host test covers loss and return, the boot-time case was benched on .61 (TASK-1999, beta.356). Real unplug/replug check: TASK-2935 (2.1.0).
<!-- SECTION:FINAL_SUMMARY:END -->
