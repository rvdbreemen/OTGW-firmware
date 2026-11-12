---
id: TASK-1833
title: >-
  OT-Direct: CE= and H2= default to off where the PIC defaults to on; CL=0
  clears instead of holding 0 %
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:41:00Z'
updated_date: '2026-11-12T01:08:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 916000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N3), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: with CL= held, MsgID 0 bit 2 = !CoolModeOff; with C2= held, bit 4 = !CH2ModeOff (readstatus G:2720-2747). Both flags 0 after reset (RAM clear G:1178-1188) -> enable bit 1 until CE=0/H2=0 (G:4446-4452, G:5264-5270). Command ref "Initial value 1". ADR-229 lines 69-70 state the same.
- OT-Direct: coolEnable = otCoolingEnable default false (O:543, :917); ch2Enable = otMasterStatusFlags & 0x10 default 0 (O:64, :919, reset O:2680-2682).
- CL=0: PIC holds 0 % (G:4454-4464; only a bad value clears, reply `-`). OT-Direct clears the override (O:3294-3307).
- Consequence: C2=40 without H2=1 switches the CH2 enable bit off; on the PIC it is forced on. Same for cooling.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host test on real otdRelayedStatusHB path: after C2=40 only bit 4 = 1; after CL=50 only bit 2 = 1; H2=0/CE=0 clear them; red on current code
- [x] #2 CL=0 holds 0 %; CL=x clears and replies `CL: -`
- [x] #3 Defaults pinned in tests/test_otd_cool_ch2_defaults.py and named in docs/pic/pic-firmware-6.8-spec.md (PIC reset state, gateway.asm:1178-1188)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): master-mode status bits (ADR-066, separate design).

Part of the PIC 6.8 parity audit, parent TASK-1851.

Ground truth checked in this session: PIC 6.8 readstatus (gateway.asm:2720-2747) comf statusflags -> bit 2 = !CoolModeOff while SysCoolLevel clear (CL held), bit 4 = !CH2ModeOff while SysCH2Setpoint clear (C2 held); SetCoolEnable/SetCentralHeat2 set the ModeOff flags; SetCoolLevel (4454-4464) holds any number incl. 0, only a non-number clears (ValueCleared). Fix: otCoolingEnable defaults true; new otCH2Enable (default true) for the relayed status, set by H2= (otMasterStatusFlags bit 4 still set for master mode, which builds its own MsgID 0: master mode unchanged); resetTransientState resets both to true; CL= parses with strtof: a number (0 too) is held, a non-number clears and replies 'CL: -'.

Tests: new tests/test_otd_cool_ch2_defaults.py (real C2/CL/CE/H2 branches, enqueue/override helpers, otdEffectiveMasterFlags + otdRelayedStatus blocks, OtdStatusCmd): old code 4 FAIL / 2 OK (the 2 pin unchanged behaviour), fix 6 OK. Harness stubs extended with otCH2Enable in test_otd_status_bits, test_otd_status_relay and test_otd_mode_switch_queue_reset (they failed to compile on the fix; OK on both old and fix after the stub). Neighbours OK: status_bits 8, status_relay, gateway_own_status 6, ch_override 9, mode_switch_queue_reset 8, vs_msgid71 6, msgid99 6, tt_pic_parity 40, alt_slots 12, ps1_summary 9. evaluate --quick 92 checks 0 FAIL. AC3 open: ADR-229 is Accepted and immutable; it states the rule (value !CoolModeOff / !CH2ModeOff) but not the reset default. Options put to Robert via the coordinator: a short successor ADR via adr-kit, or drop AC3 because the rule plus the PIC reset state already define it and the test pins the defaults.

AC3 replaced by the coordinator's decision (2026-11-12, option A): no ADR, because ADR-229 already states the rule (bit = NOT CoolModeOff / NOT CH2ModeOff) and the reset value is PIC parity, not a new decision (CLAUDE.md: no ADR for a bug fix within an existing pattern). Evidence: the defaults are pinned by tests/test_otd_cool_ch2_defaults.py (C2=40 alone -> bit 4, CL=50 alone -> bit 2) and docs/pic/pic-firmware-6.8-spec.md rows MsgID 0 bit 2/bit 4 now name the reset state; verified in gateway.asm: statusflags is RAM (line 464, CoolModeOff = bit 2, CH2ModeOff = bit 4), cleared by ClearRam (1178-1188), and the only other writer (gpio_block, 5449-5451) touches DHWBlocking only.

Full scripts/run_tests.py in wt-ba-otd was stopped by Claude Code for low system memory (not a test result). The targeted OT-Direct suites and evaluate --quick are green; the full run must be repeated and pass before the push.
<!-- SECTION:NOTES:END -->
