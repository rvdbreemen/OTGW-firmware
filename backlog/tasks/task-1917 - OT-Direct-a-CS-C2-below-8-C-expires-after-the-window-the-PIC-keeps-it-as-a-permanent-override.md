---
id: TASK-1917
title: >-
  OT-Direct: a CS=/C2= below 8 C expires after the window; the PIC keeps it as a
  permanent override
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T01:31:00Z'
updated_date: '2026-11-14T04:48:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 982000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1834 (PIC 6.8 parity audit finding b1-7, docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md). Read this session from gateway.asm 6.8: CommandExpiry :1626-1633 masks the integer part of controlsetpt1 with b'11111000'; when the HeartbeatCS flag is clear AND the value is >= 8 it sets SysCtrlSetpoint (override released); a value 1..7 skips that and stays forever. C2 (controlsetpt3) the same at :1634-1639. CS=0 is the explicit release (SetCtrlSetpoint :4218-4226 -> ClrCtrlSetPoint). PIC test tests/control-4.stc: CS=5 still R10010500 after 74 s. Our docs/pic/pic-firmware-6.8-spec.md:131 and :308 already state 'Values with HB below 8 never expire'. OT-Direct (OTDirect.ino 1-second status timer, the TASK-442 CS/C2 expiry) expires every non-zero CS/C2 after OT_CSC2_EXPIRY_MS, so a CS=5 (e.g. a deliberate low-flow / pump-only hold) is dropped after ~63 s on OT-Direct and kept on the PIC. User manuals (docs/manuals en/nl, docs/api) promise nothing about CS expiry: checked with grep 2026-11-14, no CS/C2 expiry statement found. Open for the implementer: confirm against the PIC command reference (otgw.tclcode.com firmware page) what the below-8 case is meant for before matching it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host test with a fake clock on the real expiry code: CS=5 and C2=5 at t=0 still active at t=200 s; red on the current code
- [x] #2 CS=C:/Program Files/Git/C2= >= 8 still expire as in TASK-1834 (not earlier than 63 s, gone by 64 s)
- [x] #3 the PIC's intent for values below 8 confirmed from the command reference and cited in the task
- [x] #4 docs/pic/pic-firmware-6.8-spec.md and any OT-Direct doc state the same behaviour
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 d3, lane-1917. Intent confirmed from three sources: (1) gateway.asm 6.8 CommandExpiry :1626-1639 (integer byte masked with b'11111000', release only when non-zero); (2) PIC test tests/control-4.stc 'No expiry for control setpoint below 8 degrees' (CS=5 still R10010500 after 74 s); (3) the command reference, OTGW-other-projects/OTGW-documentation/OTGW-Commands.md:493-494 (from firmware.html): 'A CS value of 8 or higher must be repeated at least every minute ... Values below 8 are maintained without the need to be repeated', plus the PIC 6.5 changelog (OTGW-Changelog.md:25) 'User control setpoints below 8 degrees are maintained without the need to be repeated once per minute'. The reference also names the use: to stop the boiler heating, set a low CS and clear CH enable. Fix: OTDirect.ino otdCsc2ExpiryStamp(f88): 0 (no expiry) when ((f88>>8) & 0xF8) == 0, else millis (never 0); the CS= and C2= handlers take their stamp from it, so the last value sent decides, as on the PIC. Evidence: new tests/test_otd_csc2_below8.py (real constant, real helper, real expiry statements, fake clock; source pin that both real handlers use the helper). Old code (origin/dev OTDirect.ino via OTGW_OTD_INO): 3 FAIL (CS=5/C2=5 gone at 74 s, 7.996 gone, handlers use millis); fix: 6/6 OK (0x0500 and 0x07FF active at 200 s; 0x0800 and 0x2D80 active at 63 s, gone at 64 s; 0xFF00 gone at 64 s). Limit: on the old code the handler rule in the test is a restatement (the handlers are not compiled), closed by the source pin. test_otd_csc2_expiry OK; all 42 tests reading OTDirect.ino OK after adding a helper stub to test_otd_cool_ch2_defaults and test_sat_command_delivery (they compile the CS/C2 handlers, not the expiry); test_sat_lowload_contract OK (35, 510 s); evaluate --quick 85/0. Docs: docs/pic/pic-firmware-6.8-spec.md:131/:308 already state it; docs/c4/c4-code-otdirect.md was stale (60 s CS/C2, 61 s SC since TASK-1834/1840): now 63.5 s and the below-8 rule. User manuals and docs/api say nothing about CS expiry (grep).

2026-11-14 edge checks (coordinator review). (a) CS=0 / C2=0 still release: both handlers check temp == 0.0f first, call clearWriteOverride(1/8) and set the stamp to 0 before otdCsc2ExpiryStamp is reached (0x0000 would otherwise pass the below-8 test). Pinned by test_zero_releases_before_the_helper (source pin on the real handler blocks); a mutant that routes the zero branch through the helper makes it FAIL. (b) Negative values expire like the PIC: -1.0 (0xFF00) and -0.5 (0xFF80) have integer byte 0xFF, masked 0xF8, so they are active at 63 s and gone at 64 s (test_negative_still_expires). Test now 7/7 on the fix; old code 4 FAIL. (c) SAT never sends a CS between 0 and 8 on OT-Direct: its CS is clamped to SAT_MIN_SETPOINT 10.0 (SATcontrol.ino:134, :1207), the cold-cut deferral returns SAT_MIN_SETPOINT - 1 = 9 (:474), and satDisable sends CS=0, the release (:825). Besides, the CS expiry is skipped while satOwnsControlSetpoint(), and SAT resends every 5 s (ADR-203/207), so the below-8 rule does not touch SAT. Not changed, noted: OT-Direct releases on temp == 0.0f while the PIC releases when the converted value is 0; a CS=0.001 (f8.8 0x0000) is therefore a permanent 0 C override on OT-Direct and a release on the PIC. Theoretical; left as is. Bench brief (optional, cheap): scratchpad bench-brief-1917.md (CS=5 stays 2 min, CS=0 releases at once, CS=45 expires at about 64 s).

2026-11-14 CS=0.001 fix (coordinator decision): with the below-8 rule a value that converts to f8.8 0 (CS=0.001) would have been a permanent 0 C override; before 1917 it at least expired after 63.5 s. The CS= and C2= release branches now test floatToF88(temp) == 0 instead of temp == 0.0f, as the PIC's SetCtrlSetpoint tests the converted bytes (gateway.asm:4218-4226: iorwf float1 / skpnz / bra ClrCtrlSetPoint). Test test_tiny_value_releases_like_zero: the real floatToF88 compiled (0.001, -0.001, 0 and 0.0039 give 0x0000, 0.004 gives 0x0001) plus a pin that both release branches use floatToF88(temp) == 0; test_zero_releases_before_the_helper now anchors on that condition. Fix 7/7; old code 5 FAIL; mutant A (zero branch stamps through the helper) 1 FAIL; mutant B (CS release back on temp == 0.0f) 2 FAIL. All 42 OTDirect tests OK, evaluate --quick 85/0. c4-code-otdirect.md and the bench brief (step 4: CS=0.001 releases at once) updated.

2026-11-14 shipped in 2.0.0-beta.236 (892218fb4, #alpha-testing post 1571023799284138104, zip sha256 e5d2a9029bd72d00e1c90847e2f5fa4b57105efb065beb66aaa781bbb7242a45). Bench by 09 on .61 (evidence 09 scratchpad b236), all 4 steps + C2 PASS: CS=5 stays 2 min; CS=0 and CS=0.001 release within about 1 s, no permanent 0; CS=45 expires at 63.84 s; C2 the same. Named limit: .61 runs in master mode without a thermostat, so the gateway-mode substitution against a real thermostat TSet was not exercised (same as TASK-1834); the expiry and release paths are proven, the substitution code is unchanged.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT-Direct now keeps a CS=/C2= below 8 C for good, like the PIC, and releases on a zero f8.8 value. AC1: tests/test_otd_csc2_below8.py on the real constant, helper and expiry code with a fake clock: CS=5/C2=5 active at 200 s; red on the old code (5 FAIL), fix 7/7, two mutants red. AC2: same test, 8.0 and 45.5 active at 63 s and gone at 64 s; test_otd_csc2_expiry still OK. AC3: intent confirmed from gateway.asm:1626-1639, tests/control-4.stc and the command reference (OTGW-Commands.md:493-494) plus the PIC 6.5 changelog. AC4: docs/pic/pic-firmware-6.8-spec.md already stated it; docs/c4/c4-code-otdirect.md updated (was 60 s/61 s). Extra: CS=0.001 release (coordinator decision), SAT unaffected (CS >= 9, never expires while SAT owns it). Gates: 42 OTDirect tests OK, test_sat_lowload_contract OK, evaluate --quick 85/0. Bench on .61 by 09 in beta.236: all steps PASS; master-mode limit named in the notes.
<!-- SECTION:FINAL_SUMMARY:END -->
