---
id: TASK-1806
title: >-
  OT-Direct: remove the 16 C flow setback on a thermostat disconnect, like the
  PIC
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-11T06:43:00Z'
updated_date: '2026-11-15T02:20:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - feature
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 899000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report A, A8 (observation) and report CD, D9. Robert's decision (2026-11-11 ~07:45, AskUserQuestion in the coordinator session, relayed): 'Like the PIC'. Today, after iSetbackTimeout (default 30 s) without thermostat frames and with FS set, checkThermostatTimeout puts setOverride(1, fSetbackTemp = 16 C) as the flow setpoint TSet with CH forced on (OTDirect.ino:2669-2678, otdEffectiveMasterFlags :827-830; default OTDirecttypes.h:112). The 'setback temperature' is a room value used as a flow setpoint, and no ADR records it (ADR-064/066/068 checked by the reviewer). The PIC has no such setback: on thermostat loss it runs its own standalone schedule with TSet 0 unless CS or a heat demand (Disconnect gateway.asm:1772-1787, StandAloneMsg :1796-1816, MessageID1 :2796-2820); its FS= is the watchdog fail-safe (SetFailSafety :5252-5259) and SB= is the GPIO away room setpoint (:5390-5398). Intended result: on thermostat loss the gateway applies no control of its own and the boiler follows its own OpenTherm timeout behaviour. Check the settings (FS, fSetbackTemp, iSetbackTimeout) for what stays, what goes and how a stored value migrates.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On thermostat loss OT-Direct no longer writes a setback flow setpoint or forces CH on; the boiler follows its own OpenTherm timeout
- [x] #2 Host test against the real checkThermostatTimeout path: red on the old code (16 C written), green on the fix
- [x] #3 The manual (EN and NL) and the release notes describe the new behaviour and what happened to the setback settings
- [ ] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (d3, lane-1924), like the PIC (Robert 2026-11-11). checkThermostatTimeout(): the setback engage block (setOverride(1, fSetbackTemp) + updateWriteCache(1), bSetbackActive) and the setback release block are removed; loss detection, otdForgetThermostatWrites() and the 'disconnected' WS status stay; the return now sends 'OTDirect: connected' on the transition (before it came from the setback release, which ran on every return in gateway mode with the default FS on). buildStatusRequest(): the 'no CS, no SAT -> CH bit 0 off' rule of master mode now applies in gateway mode too; there the scheduler sends its own MsgID 0 only once the thermostat is gone (otdOwnFramesAllowed; alternative slots never carry MsgID 0, OTDirect.ino otdAltScheduleIndex). PIC: Disconnect clears the thermostat state (gateway.asm 6.6 :1770-1789), readstatus sets CH enable only with a CS= (:2712-2742), MessageID1 sends TSet 0 unless CS= (:2796-2820). Difference kept: OT-Direct sends no MsgID 1 at all without a CS= (no write entry is valued after otdForgetThermostatWrites), the PIC writes TSet 0; with CH off the boiler does not use TSet.

Settings (decision via coordinator 2026-11-14: keep, removal is a separate task): OTDsetbacktemp, OTDfailsafe (FS=) and OTDsetbacktimeout stay stored and accepted; setbacktimeout is still the loss timeout; setback_temp and FS have no effect; REST/MQTT setback_active stays false. settingsHelp.h texts, manual EN ch09 + NL h09 (GET /api/v2/otdirect/settings note, MQTT setback_active row) updated. Dead code left on purpose for a small rebase against ba's TASK-1850: otSetbackEngaged / bSetbackActive clearing in resetTransientState, otFailSafeEnabled. AC3 part 'release notes': written by the beta flow; my share is a CHANGELOG [Unreleased] entry, added in one docs commit for all lane tasks at the rebase (the lane's CHANGELOG predates the beta.250 rollover).

AC2 evidence: new tests/test_otd_no_setback.py compiles the real checkThermostatTimeout(), otdEffectiveMasterFlags() and buildStatusRequest(); 39 s of thermostat silence in gateway mode, then a return. Fix: 4/4 OK, 'wsconn=1 ovr1=0 cache1=0 val=0000 ... lost=1 back=1 chbit=0'; with CS= chbit=1; master mode unchanged. Old (HEAD b68921bf7 copy): FAILED (failures=2), 'ovr1=1 cache1=1 val=1000' (16.0 C written) and chbit=1.

Tests updated on purpose: tests/test_otd_ch_override.py source pattern now the ownStatusRule form (master OR gateway); test/host/test_override_reply.cpp suite 1684 case P5 (gateway, no CS: own MsgID 0) expects 0x02 instead of 0x03. test/host/test_override_reply.py 4/4 suites OK; all tests/test_otd*.py OK; evaluate.py --quick 0 failed. AC3 open (CHANGELOG at rebase, release notes in the beta flow); AC4 open (build + bench). Follow-up TASK-1950 (remove the unused setback settings/fields, needs-maintainer-ok).

Lane-1924 pushed 2026-11-15: origin/dev 5da0c91df..2f4818aa1. Build esp32-combo 2.0.0-beta.281+fe0e61a OK (build/logs/build-20261115-020302.log, 'Build completed successfully'); 'git diff fe0e61a48 2f4818aa1 -- src/' empty, so the pushed code is the benched code. evaluate.py --quick 0 failed; OTD tests 42 files, host suites, SAT suite per file green on the rebased tree. Bench 09 on .61 with beta.281: GW=1 without thermostat: own MsgID 0 HB 0x00 for 55 s, no MsgID 1 write; after CS=40 HB 0x01 and one MsgID 1 write 0x2800; setback_active false; 'Thermostat disconnected' once per boot. PASS. A real thermostat loss is field-only. AC3: manual EN ch09 + NL h09 and CHANGELOG [Unreleased] done; the release notes come from the beta flow. Version beta.279.
<!-- SECTION:NOTES:END -->
