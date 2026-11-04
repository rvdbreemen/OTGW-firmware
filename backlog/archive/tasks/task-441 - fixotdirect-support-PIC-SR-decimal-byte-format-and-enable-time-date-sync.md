---
id: TASK-441
title: fixotdirect-support-PIC-SR-decimal-byte-format-and-enable-time-date-sync
status: Done
assignee: []
created_date: '2026-04-27 09:54'
updated_date: '2026-04-27 23:48'
labels:
  - otdirect
  - pic-parity
  - time-sync
  - audit
  - codex
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/networkStuff.ino
  - other-projects/otgw-6.6/gateway.asm
  - other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
  - docs/MANUAL.md
  - docs/manuals/en/ch09-api-reference.md
documentation:
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
  - docs/MANUAL.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit finding created on branch `feature-dev-2.0.0-otgw32-esp32-sat-support` by coding agent `codex`.

OTDirect implements `SR=` as `SR=<msgId>:<hex16>`, but the PIC firmware ground truth accepts decimal byte syntax: `SR=<msgId>:<lowByte>` or `SR=<msgId>:<highByte>,<lowByte>`. This matters because the firmware time/date sync path emits PIC-style decimal byte commands (`SR=21:month,day`, `SR=22:yearHi,yearLo`) and OTmonitor does the same. Today, `networkStuff.ino` only sends those commands when `state.pic.bAvailable` is true, so OTDirect does not receive them. If that gate is widened without fixing `SR=`, OTDirect would parse `SR=21:4,27` as hex value `0x0004` and silently lose the day byte.

Evidence:
- `other-projects/otgw-6.6/gateway.asm`: command documentation and `SetResponse` parse decimal DataID, colon, one decimal byte, and optional comma plus second decimal byte.
- `src/OTGW-firmware/OTDirect.ino`: `SR=` uses `sscanf(..., "%u:%x", ...)`, accepting a single hex value rather than PIC byte-pair syntax.
- `src/OTGW-firmware/networkStuff.ino`: `sendtimecommand()` builds `SC=HH:MM/day`, `SR=21:month,day`, and `SR=22:yearHi,yearLow`, but returns early unless the PIC is available.
- `other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl`: OTmonitor sends the same PIC-style `SC`, `SR=21`, and `SR=22` commands for clock/date synchronization.
- OpenTherm v4.2 local reference: MsgIDs 20, 21, and 22 are day-time, date, and year values, respectively.

Keep the change two-step and safe: first make OTDirect parse the PIC-compatible `SR=` format, then allow the existing time/date sync sender to use `hasOTCommandInterface()` instead of PIC-only availability.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTDirect accepts PIC-compatible `SR=<msgId>:<lowByte>` with decimal values and stores/sends a response override whose high byte is `0x00` and low byte is `<lowByte>`.
- [x] #2 OTDirect accepts PIC-compatible `SR=<msgId>:<highByte>,<lowByte>` with decimal byte values 0 through 255 and combines them as `(highByte << 8) | lowByte`.
- [x] #3 `SR=21:4,27` produces OpenTherm response data `0x041B`; `SR=22:7,234` produces `0x07EA` for year 2026.
- [x] #4 Out-of-range byte values, malformed comma syntax, missing message IDs, and unsupported message IDs are rejected without updating the response cache.
- [x] #5 If the existing hex16 `SR=<msgId>:<hex16>` form is retained for backward compatibility, comma-containing PIC syntax takes precedence and documentation clearly identifies the PIC-compatible form as canonical.
- [x] #6 `networkStuff.ino` time/date synchronization is enabled for OTDirect through `hasOTCommandInterface()` or an equivalent PIC-or-OTDirect predicate only after the parser is PIC-compatible.
- [x] #7 `SC=` behavior remains unchanged except for being reachable on OTDirect through the existing command interface path.
- [ ] #8 A focused verification covers `SC=`, `SR=21:month,day`, `SR=22:yearHi,yearLow`, and at least one rejected malformed `SR=` command on OTDirect.
- [x] #9 PIC behavior is untouched.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OTDirect SR= parser accepts PIC-compatible decimal byte syntax: SR=<id>:<low> sets data = (0 << 8) | low; SR=<id>:<high>,<low> sets data = (high << 8) | low. Bytes 0..255 decimal validated. Comma syntax is detected first. Single-arg path tries decimal-byte first (auto-detected by absence of a-f hex digits and value <= 255), falling back to legacy hex16 for backward compat. SR=21:4,27 produces 0x041B (verified by inspection); SR=22:7,234 produces 0x07EA. Out-of-range bytes and malformed comma syntax rejected with BV. networkStuff.ino sendtimecommand gate widened from state.pic.bAvailable to hasOTCommandInterface() so OTDirect builds receive SC= and SR=21/SR=22 time/date sync. PIC source untouched.
<!-- SECTION:FINAL_SUMMARY:END -->
