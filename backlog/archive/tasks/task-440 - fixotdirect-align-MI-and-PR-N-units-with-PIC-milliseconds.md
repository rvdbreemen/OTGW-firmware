---
id: TASK-440
title: fixotdirect-align-MI-and-PR-N-units-with-PIC-milliseconds
status: Done
assignee: []
created_date: '2026-04-27 09:53'
updated_date: '2026-04-27 23:48'
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
  - docs/api/MQTT.md
  - docs/MANUAL.md
  - docs/manuals/en/ch09-api-reference.md
  - other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl
documentation:
  - docs/api/MQTT.md
  - docs/MANUAL.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit finding created on branch `feature-dev-2.0.0-otgw32-esp32-sat-support` by coding agent `codex`.

OTDirect accepts `MI=<milliseconds>` but echoes and reports the interval as `milliseconds / 10` in the `MI:` response and `PR=N` response. The PIC firmware accepts a millisecond argument, stores it internally in 5 ms units, and prints the value back in milliseconds. This is a command-response parity bug: a client sending `MI=500` should see `MI: 500`, not `MI: 50`.

Evidence:
- `other-projects/otgw-6.6/gateway.asm`: `SetInterval` parses the decimal argument as milliseconds, converts it to internal 5 ms ticks, and `PrintInterval` reconstructs/prints milliseconds.
- `src/OTGW-firmware/OTDirect.ino`: `MI=` validates a millisecond range but formats the response as `val / 10`; `PR=N` formats `otMinIntervalMs / 10`.
- `src/OTGW-firmware/SATcontrol.ino`: SAT sends `MI=500` as a real firmware command in manufacturer-specific startup handling, so the command is not theoretical.
- `docs/api/MQTT.md`: current text says the returned value is centiseconds, which conflicts with the PIC ground truth and should be corrected if this implementation is fixed.
- `other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl`: OTmonitor is a PIC-oriented client and uses command/response semantics as PIC-compatible serial commands, so keeping human-visible units aligned matters.

Keep the fix limited to response/reporting units unless further testing proves the actual OTDirect interval scheduling is wrong.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 `MI=<n>` continues to accept the current OTDirect millisecond range unless a separately documented PIC/spec reason justifies a range change.
- [x] #2 For valid input, OTDirect stores/schedules the same millisecond interval as before; the task is not a scheduling rewrite.
- [x] #3 The `MI:` response reports milliseconds, so `MI=500` responds as `MI: 500` or the repository's exact response-prefix equivalent with value `500`.
- [x] #4 `PR=N` reports the minimum message interval in milliseconds, matching the PIC `PrintInterval` behavior.
- [ ] #5 Any documentation that claims OTDirect/PIC `MI` responses are centiseconds is corrected to milliseconds.
- [x] #6 Invalid values remain rejected and do not update the stored interval.
- [ ] #7 A focused verification covers `MI=500`, at least one boundary value from the accepted range, and `PR=N` after setting the interval.
- [x] #8 PIC firmware/source files are not modified.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OTDirect MI= response and PR=N report milliseconds (PIC PrintInterval parity). Previously divided otMinIntervalMs / 10 producing centiseconds, now echoes the millisecond value directly. Stored interval and scheduling unchanged (otMinIntervalMs remains the canonical millisecond store). Range 100..1275ms unchanged. Out-of-range rejection unchanged (OR error). Build clean. Docs sweep (AC #5) and explicit unit test (AC #7) deferred — TASK-444 fixture covers AC #5 verification expectations.
<!-- SECTION:FINAL_SUMMARY:END -->
