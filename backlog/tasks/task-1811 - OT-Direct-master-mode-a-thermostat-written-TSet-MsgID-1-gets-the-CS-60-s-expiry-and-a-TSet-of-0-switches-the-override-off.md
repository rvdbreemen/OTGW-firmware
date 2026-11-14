---
id: TASK-1811
title: >-
  OT-Direct master mode: a thermostat-written TSet (MsgID 1) gets the CS= 60 s
  expiry and a TSet of 0 switches the override off
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T12:28:00Z'
updated_date: '2026-11-14T05:03:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 904000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report E, E-cand 4. Robert's decision (2026-11-11, AskUserQuestion in the coordinator session): "Expiry + TSet=0 respected": a TSet written by the thermostat in master mode gets the same expiry as CS=, and TSet=0 switches off. Code today (dev 755203ef0): handleMasterModeSlaveFrame (OTDirect.ino:2801-2848) calls setOverride(1, v) + updateWriteCache(1, v) only for a WRITE of MsgID 1 > 0 while SAT does not own the setpoint (:2825-2828); a write of 0 is skipped. The 60 s heartbeat (OT_CSC2_EXPIRY_MS :502) runs only when otCSLastCommandMs != 0 (:2359-2364), and only the CS= command sets that timer (:2947); CS=0 clears the override and the timer (:2942). So a thermostat TSet never expires, and the thermostat's TSet 0 (stop demand) is acknowledged but has no effect: the boiler keeps the last TSet and CH stays enabled (buildStatusRequest enables CH while a CS override exists) until a reboot, CS=0 or a mode change. The PIC clears override and setpoints on a thermostat disconnect (gateway.asm:1782-1784, area A B6). docs/BREAKING_CHANGES.md:25 documents "a later write of 0 is ignored" and must change with the fix. TASK-1150 (SAT owns the setpoint: thermostat write ignored) stays. Not decided by Robert and left out of the ACs: report E also notes that the thermostat's MsgID 16 and 24 writes (updateWriteCache :2830-2836) are re-sent forever after the thermostat goes silent; raise it with Robert if the fix touches that path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 In master mode with SAT off, a thermostat WRITE of MsgID 1 > 0 sets the override and refreshes the same 60 s expiry timer as CS=; without a new write the override clears after OT_CSC2_EXPIRY_MS and CH follows as after a CS= expiry
- [x] #2 In master mode with SAT off, a thermostat WRITE of MsgID 1 = 0 clears the override like CS=0
- [x] #3 While SAT owns the control setpoint the thermostat write still changes nothing (TASK-1150 unchanged)
- [x] #4 docs/BREAKING_CHANGES.md:25 describes the new behaviour (expiry and TSet 0) instead of "a later write of 0 is ignored"
- [x] #5 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #6 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 d3, lane-1811. Below-8 question (TASK-1917 came after Robert's decision): coordinator decision A, recorded as its reading of Robert's 2026-11-11 decision, not a new one from Robert: a thermostat TSet always expires after OT_CSC2_EXPIRY_MS (63.5 s) whatever its value; the below-8 permanence is the PIC's semantics for a deliberate user CS= (CS + CH=0 to stop heating), not for a thermostat write, and a vanished thermostat must never leave a stuck setpoint. Fix in handleMasterModeSlaveFrame (OTDirect.ino): WRITE MsgID 1 while SAT does not own the setpoint: 0 -> clearWriteOverride(1) + otCSLastCommandMs = 0 (like CS=0); > 0 -> setOverride + updateWriteCache + otCSLastCommandMs = millis() (never 0), a separate path from otdCsc2ExpiryStamp. The existing 1-second expiry then clears it (and CH follows, as after a CS= expiry). Note: a thermostat write of 0 also ends a user CS= override; a thermostat write > 0 already replaced it before this change. Evidence: new tests/test_otd_master_tset_expiry.py, compiling the real buildOTResponse, handleMasterModeSlaveFrame and expiry statements with a fake clock: TSet 45.5 at 1 s active at 64 s, gone at 66 s; TSet 5.0 likewise gone at 66 s and otdCsc2ExpiryStamp never called; TSet 0 at 10 s ends it at 10 s; SAT owner: no override. Old code (origin/dev): 3 FAIL (SAT case passes on both, unchanged as AC3 asks). Mutant (thermostat path through otdCsc2ExpiryStamp): 2 FAIL. All 43 tests reading OTDirect.ino OK; evaluate --quick 85/0. Docs: BREAKING_CHANGES.md master-mode paragraph (also the stale 60 s -> 63.5 s and the CS below 8 rule), c4-code-otdirect.md handler section. Not in scope, as the task says: thermostat MsgID 16/24 writes are still re-sent after the thermostat goes silent.
<!-- SECTION:NOTES:END -->
