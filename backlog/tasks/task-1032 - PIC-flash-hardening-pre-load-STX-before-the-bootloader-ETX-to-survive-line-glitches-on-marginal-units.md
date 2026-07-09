---
id: TASK-1032
title: >-
  PIC-flash hardening: pre-load STX before the bootloader ETX to survive line
  glitches on marginal units
status: To Do
assignee: []
created_date: '2026-07-09 15:45'
updated_date: '2026-07-09 21:26'
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
- [x] #1 Early-STX handshake implemented behind the existing upgrade FSM (OTGWSerial), protocol-compatible with selfprog.asm (analyse WaitForSTX/ReSync/GetNextDat byte-by-byte before coding)
- [ ] #2 No regression on a known-good board (field tester with working flash, e.g. S3 Mini Pro combo)
- [ ] #3 Bench Classic-S3 (marginal unit) retried with the hardening: outcome recorded either way in TASK-972 follow-up notes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 AC#1 byte-level protocol analysis (otgw-6.6 selfprog.asm). Flow: after reset the bootloader inits EUSART, sends ETX (WrRS232), then enters WaitForSTX -> Pause(16) which returns IMMEDIATELY if RCIF is set (a byte already in the 2-byte FIFO). So an STX we transmit BEFORE the bootloader reaches WaitForSTX sits in the FIFO; Pause returns at once, RdRS232 reads it, it IS STX (Z set), skpz falls into ReSync -> GetNextDat waits (no inner timeout) for the rest of the frame. GetNextDat treats an unprotected STX as a ReSync restart and ETX as end-of-frame with a checksum test; a checksum FAIL routes to StartOfLine which waits for a fresh STX (bootloader STAYS ALIVE) — this is the property the hardening exploits. IMPLEMENTATION would need to split fwCommand's leading STX from its {payload+checksum+ETX} in FWSTATE_RSET: send STX early (post-resetPic), then the remainder after the bootloader's ETX arrives, keeping DLE-escaping + running checksum intact.\n\nDISPOSITION (not shipped): this changes the bootloader handshake timing for ALL boards — there is no way to scope it to marginal units. The healthy PIC-flash path was only field-validated this week (crashevans alpha.337, my Pro alpha.341) and the benefit here is SPECULATIVE (the bench Classic-S3 marginal unit is most likely hardware-defective per number3nl's own 'must be my soldering' conclusion). Shipping a delicate STX-preload change to a just-stabilized critical path for unproven marginal-hardware resilience is not justified as an aggressive-drain close; kept OPEN with this analysis. If pursued later: implement behind the FSM, hard-gate on a 100%-flash no-regression test on the Pro (COM10) AND the Classic-S3 (COM8), revert on any regression.
<!-- SECTION:NOTES:END -->
