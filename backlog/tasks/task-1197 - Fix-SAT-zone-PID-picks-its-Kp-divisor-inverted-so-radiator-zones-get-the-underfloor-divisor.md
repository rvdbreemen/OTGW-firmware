---
id: TASK-1197
title: >-
  Fix: SAT zone PID picks its Kp divisor inverted, so radiator zones get the
  underfloor divisor
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 07:25'
updated_date: '2026-10-03 07:38'
labels:
  - sat
  - bug
dependencies: []
priority: medium
ordinal: 319000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while building the TASK-1195 harness (2026-10-03), from reading the code; not reproduced yet.
satZonePidStep() (src/OTGW-firmware/SATcontrol.ino:1617) computes the divisor as (settings.sat.iHeatingSystem == 1) ? 4.0f : 3.0f. SAT_HSYS_RADIATORS is 1 (SATtypes.h:42), so:
- a radiator zone gets 4, the underfloor divisor;
- an underfloor zone (2) gets 3;
- AUTO (0) gets 3, correct by chance, because AUTO resolves to radiators.
The primary PID uses (satGetEffectiveHeatingSystem() == SAT_HSYS_UNDERFLOOR) ? SAT_PID_KP_DIVISOR_FLOOR (4) : SAT_PID_KP_DIVISOR_RAD (3), at SATpid.ino _pidCalculateGains(). That matches pid.py kp(): '4 if UNDERFLOOR else 3' (pid.py:70).
TASK-891.8 fixed this inversion in the primary PID only. The zone comment ('Constants mirror SATpid.ino') still describes the intended mapping.
Effect: Kp, and with it Ki = Kp/8400, is 25% low for radiator zones and 33% high for underfloor zones.
Context, not part of this task: the zone PID is P+I only. pid.py's area PIDs use the full PID class, D term included.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satZonePidStep() selects the divisor the same way as _pidCalculateGains() and pid.py: 4 for underfloor, 3 otherwise, with AUTO resolved through satGetEffectiveHeatingSystem()
- [x] #2 Old-vs-fix host proof in test/host/test_sat_pid_derivative.py, which already slices the zone step: a case derives the zone Kp from the zone output for RADIATORS, UNDERFLOOR and AUTO. The old code fails RADIATORS and UNDERFLOOR; the fix passes all three; a mutant restoring '== 1' fails the case
- [x] #3 Prerelease bump in the same commit; build.bat esp32-combo green with a fresh firmware.bin; python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. SATcontrol.ino satZonePidStep() (line 1617): divisor = (satGetEffectiveHeatingSystem() == SAT_HSYS_UNDERFLOOR) ? 4.0f : 3.0f. satGetEffectiveHeatingSystem() is the static at line 194, defined earlier in the same file. The SAT_PID_KP_DIVISOR_* constants live in SATpid.ino, which comes later in the sketch, so the literals stay; the existing comment names them. One line plus the comment.
2. Proof: test/host/test_sat_pid_derivative.py, case Z1. It derives the zone Kp from the zone output for AUTO, RADIATORS and UNDERFLOOR. Run with --old-rev edb5c5cfe --defects Z1: OLD fails only Z1 and keeps the pid.py rules; FIX passes every check. A new mutant MZ, which restores '== 1', must fail Z1. The runner's mutants get a target file for that (SATcontrol.ino).
3. Same commit: bump to alpha.404, then build.bat esp32-combo, evaluate.py --quick and push.
Red already run on alpha.403: Z1 FAIL, with auto kp 20 (want 20), radiators kp 15 (want 20) and underfloor kp 20 (want 15).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
REPRODUCED 2026-10-03 on alpha.403 (edb5c5cfe). Command: python test/host/test_sat_pid_derivative.py --report worktree. Case Z1, with coeff 1.5 and curve 40: auto kp 20.00 (want 20.00), radiators kp 15.00 (want 20.00), underfloor kp 20.00 (want 15.00). Every other check passes.

EVIDENCE 2026-10-03, committed revisions. Command: python test/host/test_sat_pid_derivative.py --old-rev edb5c5cfe --rev HEAD --defects Z1 (HEAD = 1d9ae71ae, alpha.404). Exit 0.
- OLD (edb5c5cfe, alpha.403): Z1 FAIL. auto kp 20.00 (want 20.00), radiators kp 15.00 (want 20.00), underfloor kp 20.00 (want 15.00). The pid.py rules (C1, C2, C2b, C3, C5a, C6, C7, H1) pass.
- FIX (HEAD): all checks pass. Z1 radiators 20.00, underfloor 15.00, auto 20.00.
- Mutants: MZ (iHeatingSystem == 1 restored) fails Z1. MF, MC, MA, MB, MR and MRz still fail their own cases.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
satZonePidStep() picked its Kp divisor with iHeatingSystem == 1 ? 4 : 3. Radiator zones (1) got the underfloor divisor and underfloor zones the radiator one: Kp, and Ki with it, was 25% low on radiators and 33% high on underfloor. It now uses satGetEffectiveHeatingSystem() == SAT_HSYS_UNDERFLOOR ? 4 : 3, as _pidCalculateGains() and pid.py kp() do (commit 1d9ae71ae, alpha.404, pushed to origin/dev).

Evidence per AC:
- AC#1: the divisor line in SATcontrol.ino satZonePidStep(). Harness case Z1 passes on HEAD: radiators Kp 20, underfloor 15, auto 20, with coeff 1.5 and curve 40.
- AC#2: python test/host/test_sat_pid_derivative.py --old-rev edb5c5cfe --rev HEAD --defects Z1, exit 0. OLD fails only Z1 (radiators 15, underfloor 20) and keeps the pid.py rules. FIX passes every check. The new mutant MZ (== 1 restored) fails Z1. The runner's mutants now name their source file.
- AC#3: bump to alpha.404 in the same commit. build.bat --target esp32-combo (PowerShell): 'Successfully created ESP32S3 image', firmware SUCCESS 2:22, filesystem SUCCESS 0:25, fresh OTGW-firmware-esp32-combo-2.0.0-alpha.404+1d9ae71 artifacts (09:36). python evaluate.py --quick: 71 passed, 0 warnings, 0 failed.

Behaviour change for testers: only multi-zone SAT (zone count > 1); single-zone is unchanged. Not in scope: the zone PID has no D term, while pid.py's area PIDs do.
<!-- SECTION:FINAL_SUMMARY:END -->
