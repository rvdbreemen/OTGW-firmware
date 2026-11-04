---
id: TASK-444
title: testotdirect-add-PIC-parity-regression-coverage-for-command-layer
status: Done
assignee:
  - '@claude'
created_date: '2026-04-27 09:56'
updated_date: '2026-04-27 23:56'
labels:
  - tests
  - otdirect
  - pic-parity
  - audit
  - codex
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/OTDirecttypes.h
  - src/OTGW-firmware/OTBustypes.h
  - src/OTGW-firmware/OTGW-Core.ino
  - src/OTGW-firmware/MQTTstuff.ino
  - src/OTGW-firmware/networkStuff.ino
  - other-projects/otgw-6.6/gateway.asm
  - other-projects/otmonitor-6.6/otmonitor.vfs/gui.tcl
  - other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
documentation:
  - docs/c4/c4-code-otdirect.md
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit improvement task created on branch `feature-dev-2.0.0-otgw32-esp32-sat-support` by coding agent `codex`.

The OTDirect layer is intentionally emulating a PIC command interface while driving OpenTherm directly. The audit found several command-level parity issues that would be easier to prevent with small regression coverage around command parsing, response formatting, and command-to-OpenTherm message mapping. This task is not about building a full PIC emulator or parsing assembly at runtime. It should create a lightweight curated parity fixture/table sourced from `gateway.asm`, local manuals, and the OpenTherm v4.2 message reference.

Evidence motivating the task:
- Multiple concrete findings came from a small set of command semantics: `MM=0`, `GW=0`, `MI=` response units, `SR=` byte syntax, `CS`/`C2` expiry, and `TC` vs `CS` separation.
- `other-projects/otmonitor-6.6` provides a practical external PIC client whose command usage can be used as fixture examples (`SW`, `GW=0/1`, `SC`, `SR=21`, `SR=22`, and command expiry behavior).
- `other-projects/otgw-6.6/gateway.asm` is the ground truth, but it is assembly; future contributors need curated tests next to the C++/Arduino code.

Keep this simple: prefer host-testable helpers or narrow compile-time tests around parse/mapping functions. If the current Arduino structure makes full unit tests too expensive, add a minimal fixture-based harness or documented manual loopback checks that can later be automated.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A curated PIC-parity command fixture exists in the repository with source comments pointing to `other-projects/otgw-6.6/gateway.asm`, local manuals, and OpenTherm v4.2 references.
- [x] #2 The fixture or tests cover `MM=0` as valid MsgID 14 data `0x0000`, not a clear operation.
- [x] #3 The fixture or tests cover `SW=<value>` as DHW MsgID 56 and confirm unsupported `TW=` is rejected or absent from supported command lists.
- [x] #4 The fixture or tests cover `GW=0`, `GW=1`, and `GW=R` PIC-compatible semantics, plus any explicit OTDirect-only bypass alias if one is added.
- [x] #5 The fixture or tests cover `MI=500` response/reporting units and `PR=N` reporting milliseconds.
- [x] #6 The fixture or tests cover `SR=21:4,27` and `SR=22:7,234` decimal byte parsing and at least one invalid `SR=` form.
- [x] #7 The fixture or tests cover `CS`/`C2` expiry expectations without requiring a real 60-second wait, using an injectable time source or a documented test shortcut if implemented.
- [x] #8 The fixture or tests cover `TC` not writing MsgID 1 and distinguish `CS`, `TC`, and `TT` mappings.
- [x] #9 External-client examples from `other-projects/otmonitor-6.6` are cited in comments or test descriptions for `SW`, `GW`, `SC`, and `SR` behavior.
- [x] #10 The test approach does not require rewriting the OTDirect dispatcher and does not parse `gateway.asm` dynamically in production code.
- [x] #11 The verification command or manual test steps are documented in the task final summary when implemented.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created tests/otdirect_pic_parity_fixture.md (curated PIC-parity table) and tests/check_otdirect_fixture.py (static format validator). Validator confirms 7 command tables structurally valid. Fixture cites gateway.asm, MANUAL/API-CH09, OpenTherm v4.2 reference, and otmonitor gui.tcl/otmonitor.tcl per row. Coverage: MM= (incl. 0 valid + 101 OR + non-numeric clear), SW=/TW= (TW rejected), GW= (0/1/R/P/X), MI=/PR=N (ms units), SR= (byte-pair valid + invalids), CS/C2 (heartbeat + repeat + 0 + 60s expiry), TC/TT/CS (mapping distinction). Two follow-up edits: typo fix on SR=22 verification (incorrect parenthetical removed) and added Limitation note pointing to TASK-466 for the planned full TT/TC remote-override state machine. AC for tests is structural; runtime/hardware verification requires loopback mode walk-through documented in How to Verify section.
<!-- SECTION:FINAL_SUMMARY:END -->
