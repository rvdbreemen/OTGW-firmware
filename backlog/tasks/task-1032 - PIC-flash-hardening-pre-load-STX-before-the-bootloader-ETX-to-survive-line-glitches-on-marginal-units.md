---
id: TASK-1032
title: >-
  PIC-flash hardening: pre-load STX before the bootloader ETX to survive line
  glitches on marginal units
status: To Do
assignee: []
created_date: '2026-07-09 15:45'
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
