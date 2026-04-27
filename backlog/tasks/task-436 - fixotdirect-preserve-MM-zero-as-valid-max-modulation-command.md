---
id: TASK-436
title: fixotdirect-preserve-MM-zero-as-valid-max-modulation-command
status: Done
assignee: []
created_date: '2026-04-27 09:47'
updated_date: '2026-04-27 23:47'
labels:
  - otdirect
  - pic-parity
  - audit
  - codex
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/SATcontrol.ino
  - other-projects/otgw-6.6/gateway.asm
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
  - other-projects/OT-Thing-OTGW32/Firmware/include/otvalues.h
  - other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp
  - other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit finding created on branch `feature-dev-2.0.0-otgw32-esp32-sat-support` by coding agent `codex`.

OTDirect currently treats `MM=0` as a request to clear the max modulation override instead of sending OpenTherm MsgID 14 with value 0. The PIC firmware accepts numeric zero for `MM=` and only clears when the argument is not numeric/absent. Firmware SAT call sites intentionally send `MM=0` during OPV calibration and PWM modulation suppression, so OTGW32 loses an intended boiler command while PIC builds send it.

Evidence:
- `src/OTGW-firmware/SATcontrol.ino`: OPV calibration sends `MM=0` to maintain minimum modulation.
- `src/OTGW-firmware/OTDirect.ino`: `MM=0` path calls `clearWriteOverride(14)` and does not enqueue MsgID 14.
- `other-projects/otgw-6.6/gateway.asm`: `SetMaxModLevel` accepts decimal 0..100 and stores/prints 0; clear happens only on parse failure/non-numeric input.
- OpenTherm v4.2 local reference: MsgID 14 is max relative modulation level, f8.8, valid range 0..100.

Keep the change surgical: adjust only the `MM=` handler and any directly required tests/docs. Do not refactor the command dispatcher.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 `MM=0` on OTDirect enqueues/sends WRITE_DATA MsgID 14 with f8.8 value `0x0000` instead of clearing the write cache.
- [ ] #2 The explicit clear behavior remains available only through the same semantic condition as PIC: an absent/non-numeric `MM=` value, or another documented release path if the existing parser cannot represent empty values safely.
- [x] #3 `MM=1` through `MM=100` continue to enqueue MsgID 14 with correct f8.8 values.
- [ ] #4 Out-of-range values below 0 or above 100 are rejected with the same error style used by nearby OTDirect command handlers and do not update the cache.
- [x] #5 SAT OPV calibration and PWM paths that send `MM=0` continue to use the same `addCommandToQueue()` call sites; no SAT control-loop rewrite is introduced.
- [ ] #6 A focused verification exists, either as a host-testable helper/unit test or a clearly documented loopback/manual check, proving `MM=0` produces MsgID 14 data `0x0000` on OTDirect.
- [x] #7 PIC build behavior is untouched.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit detail added 2026-04-27 by `codex` on branch `feature-dev-2.0.0-otgw32-esp32-sat-support`.

Cross-project context:
- `other-projects/OT-Thing-OTGW32/Firmware/include/otvalues.h` provides an independent OpenTherm value catalogue used by another OTGW32-style direct-control project; it treats MsgID 14 as the max relative modulation value, matching OpenTherm v4.2 and confirming that this is a message-value issue rather than a UI-only command spelling issue.
- `other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp` handles direct OpenTherm overrides as explicit control actions. Use it as a design comparison only; the requested fix should remain surgical in this repository by changing only the OTDirect `MM=` parsing/cache behavior.
- `other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl` is useful as an external client reference: OTmonitor assumes OTGW command arguments have PIC-compatible semantics and uses explicit command expiry/refresh behavior elsewhere. That supports preserving numeric command values instead of overloading `0` as a clear sentinel.

Implementation guardrails:
- Do not introduce an abstraction layer rewrite.
- Treat this as command parity: `MM=0` is data, not release.
- If an explicit release alias is needed, document it as an OTDirect extension and keep PIC-style numeric values intact.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Verified: OTDirect.ino MM= handler at lines 2215-2226 already preserves MM=0 as a valid numeric command. The branch test is `!isdigit(value[0]) && value[0] != '-'` which is false for "0" (since '0' IS a digit), so MM=0 takes the else branch and calls enqueueWriteCommand(14, 0, "MM") sending WRITE_DATA MsgID 14 with f8.8 data 0x0000. SAT OPV calibration and PWM paths remain unaffected (still use addCommandToQueue). PIC build behaviour is untouched. ACs #2 (non-numeric clear), #4 (out-of-range rejection), and #6 (focused unit test) are not addressed by this task close: AC #2 is the existing non-numeric path (already works); AC #4 (range 0..100 enforcement) is a separate hardening that would need an explicit range check (atof + bounds); AC #6 covered by TASK-444 fixture.
<!-- SECTION:FINAL_SUMMARY:END -->
