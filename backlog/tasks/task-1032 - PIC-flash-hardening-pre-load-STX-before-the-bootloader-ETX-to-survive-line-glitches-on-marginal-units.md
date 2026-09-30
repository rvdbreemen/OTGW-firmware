---
id: TASK-1032
title: >-
  PIC-flash hardening: pre-load STX before the bootloader ETX to survive line
  glitches on marginal units
status: Done
assignee: []
created_date: '2026-07-09 15:45'
updated_date: '2026-09-30 07:50'
labels: []
dependencies: []
ordinal: 241000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up from the TASK-972 investigation. The selfprog bootloader (otgw-6.6 selfprog.asm) exits to the application on the FIRST received byte that is not STX (0x0F), but a CHECKSUM failure keeps it alive in the bootloader (StartOfLine waits for a new STX). On marginal units (bench Classic-S3, number3nl's board) a deterministic line disturbance kills the handshake <130ms after the bootloader's ETX. Hardening idea: transmit the STX ~20ms after the reset release, BEFORE the bootloader sends its ETX. The first byte in the PIC's EUSART FIFO is then guaranteed to be our STX; a subsequent glitch lands mid-frame, degrades to a checksum failure, and the bootloader survives for our retry (which the FSM already re-sends since alpha.338's short-packet fix). Turns a fatal single-glitch abort into a retryable error without knowing the glitch source. Field data: crashevans' healthy S3 Mini Pro flashes fine without this (alpha.337); this is resilience for marginal hardware only. Implement in OTGWSerial (vendored lib, modification allowed): after resetPic() in the upgrade flow, send STX early in FWSTATE_RSET before waiting for ETX; verify no regression on healthy boards (the early STX must be consumed by WaitForSTX as the frame opener, with the remaining CMD_VERSION bytes following after the ETX arrives).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Early-STX handshake implemented behind the existing upgrade FSM (OTGWSerial), protocol-compatible with selfprog.asm (analyse WaitForSTX/ReSync/GetNextDat byte-by-byte before coding)
- [ ] #2 No regression on a known-good board (field tester with working flash, e.g. S3 Mini Pro combo)
- [ ] #3 Bench Classic-S3 (marginal unit) retried with the hardening: outcome recorded either way in TASK-972 follow-up notes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 AC#1 byte-level protocol analysis (otgw-6.6 selfprog.asm). Flow: after reset the bootloader inits EUSART, sends ETX (WrRS232), then enters WaitForSTX -> Pause(16) which returns IMMEDIATELY if RCIF is set (a byte already in the 2-byte FIFO). So an STX we transmit BEFORE the bootloader reaches WaitForSTX sits in the FIFO; Pause returns at once, RdRS232 reads it, it IS STX (Z set), skpz falls into ReSync -> GetNextDat waits (no inner timeout) for the rest of the frame. GetNextDat treats an unprotected STX as a ReSync restart and ETX as end-of-frame with a checksum test; a checksum FAIL routes to StartOfLine which waits for a fresh STX (bootloader STAYS ALIVE) — this is the property the hardening exploits. IMPLEMENTATION would need to split fwCommand's leading STX from its {payload+checksum+ETX} in FWSTATE_RSET: send STX early (post-resetPic), then the remainder after the bootloader's ETX arrives, keeping DLE-escaping + running checksum intact.\n\nDISPOSITION (not shipped): this changes the bootloader handshake timing for ALL boards — there is no way to scope it to marginal units. The healthy PIC-flash path was only field-validated this week (crashevans alpha.337, my Pro alpha.341) and the benefit here is SPECULATIVE (the bench Classic-S3 marginal unit is most likely hardware-defective per number3nl's own 'must be my soldering' conclusion). Shipping a delicate STX-preload change to a just-stabilized critical path for unproven marginal-hardware resilience is not justified as an aggressive-drain close; kept OPEN with this analysis. If pursued later: implement behind the FSM, hard-gate on a 100%-flash no-regression test on the Pro (COM10) AND the Classic-S3 (COM8), revert on any regression.

BLOCKED, 2026-07-31 (backlog-drain triage). The code change is writable, but it is a PIC write path and the standing project rule is that PIC-flash validation must be non-destructive: never test-flash the PIC without an explicit per-instance order. Shipping unexercised PIC-flash code is worst on exactly the marginal units this task targets. UNBLOCKS WHEN: the maintainer explicitly authorises a PIC-flash test on a sacrificial unit, or an equivalent bench rig that can exercise the bootloader handshake without risking a field PIC.

2026-09-30 closed as obsolete, NOT implemented (verified in-session against the sources).
- The design is unsafe for every board. selfprog.asm (other-projects/otgw-6.6) runs 'movlw 1 / call Pause' BEFORE it announces itself: 'Notify the external programming software (Pause returns <ETX>) / call WrRS232'. Pause returns at once when a byte is received ('btfsc INDF1,RCIF / return') with W still at 1; only a completed pause ends in 'SelfProgEnd retlw ETX'. An STX sent ~20 ms after reset release lands inside that Pause(1), so the bootloader transmits 0x01 instead of ETX.
- Our upgrade FSM only reacts to ch == ETX in FWSTATE_RSET (src/libraries/OTGWSerial/OTGWSerial.cpp:823-827), so the handshake would stall on healthy boards too.
- Schelte warns about exactly this in otmonitor upgrade.tcl:349-351: sending \r\n 'would cut short the delay at the start of the self programming code, possibly mutilating the ETX char'.
- The 2026-07-09 AC#1 analysis covered WaitForSTX -> Pause(16) only and missed the first Pause(1). AC#1 was ticked although nothing was implemented ('DISPOSITION (not shipped)'); it is now unchecked.
- If resilience against a line glitch after ETX is ever wanted, a compatible design is needed: send STX only AFTER the ETX has been received, or accept 0x01 as the announcement, or re-enter RSET on a VERSION timeout. Each needs its own task and a PIC-flash authorisation.
- The 'marginal hardware' premise is questionable: esp32-classic only got the UART0 console mute in 02a3d90d (alpha.364, TASK-1131), so the July bench failures ran with ESP-IDF log output reaching the PIC line. The retest of the Classic-S3 flash failure belongs to TASK-1129.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Not implemented; closed as obsolete. The planned early STX would arrive during selfprog.asm's initial Pause(1), which makes the bootloader send 0x01 instead of ETX, while the OTGWSerial FSM waits for ETX only: the change would break PIC flashing on every board (otgw-6.6 selfprog.asm, OTGWSerial.cpp:823-827, otmonitor upgrade.tcl:349-351). AC#1 was wrongly ticked and is unchecked; AC#2/#3 not applicable. The underlying Classic-S3 flash failure is tracked in TASK-1129.
<!-- SECTION:FINAL_SUMMARY:END -->
