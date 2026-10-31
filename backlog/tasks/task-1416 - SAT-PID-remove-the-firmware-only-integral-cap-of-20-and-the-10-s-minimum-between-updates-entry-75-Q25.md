---
id: TASK-1416
title: >-
  SAT PID: remove the firmware-only integral cap of +-20 and the 10 s minimum
  between updates (entry 75, Q25)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T06:45:00Z'
updated_date: '2026-10-30T21:45:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
  - TASK-1270
references:
  - docs/sat/sergeant-d-corrections.md
documentation:
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
priority: medium
ordinal: 580000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 75, Q25: follow thermo-nova). Remove SAT_PID_INTEGRAL_ABS_MAX (SATpid.ino ~199-202), the zone +-20 cap (SATcontrol.ino ~2162-2163), SAT_PID_SAMPLE_TIME_LIMIT and its gate (SATpid.ino ~21, ~299-304). The unchanged-error gate stays with TASK-1368. Update ADR-197 Context 2 / Decision 3.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: integral exceeds 20 when thermo-nova's would (red on HEAD, green on fix)
- [x] #2 Two updates 5 s apart are both processed when the 60 s cadence allows (no 10 s gate)
- [x] #3 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Execution-graph update requested by Robert: serialize pending SATpid.ino writers after TASK-1368 -> TASK-1270: TASK-1416 -> TASK-1417 -> TASK-1418 -> TASK-1414 -> TASK-1423 -> TASK-1415. TASK-1381 remains a parallel branch after TASK-1270, gated separately by Q53. Revalidate the task premise after the preceding fix; if already addressed, record evidence and re-scope/close through the normal review, rather than applying a duplicate change. Persistence write cadence remains Robert's decision.

2026-10-30 (otgw-firmware-84, wt-1416, local commit a2942c547 on origin/dev 0772659cf, no bump). Premise revalidated: the 10 s gate (SAT_PID_SAMPLE_TIME_LIMIT) was already removed by TASK-1368 (8b2f5f7f9); only the +-20 caps remained (SATpid.ino _pidUpdateIntegral, SATcontrol.ino satZonePidAdvance) and are removed. thermo-nova opened at the pin: pid.py:159-171, :185-192, helpers.py:148-150, area.py:208. Evidence: tests/test_sat_pid_integral_cap.py 5/5 OK on the fix; with origin/dev SATpid.ino + SATcontrol.ino FAILED (failures=4): primary 20.0 vs 22.857 (3000 steps) and 40.0 (10000 steps), zone 20.0 vs 33.9. AC#2: two steps 5 s apart both move the integral (green on dev too, the TASK-1368 fix). All tests/test_sat_*.py exit 0; evaluate --quick 79 passed / 0 warnings / 0 failed. AC#3 build: open, comes with the coordinator's batch build. ADR-197 (Accepted) not edited: it already states the rule and names the cap as a TASK-1416 difference until Done.

2026-10-30 close-out: merged on origin/dev as b28f57ca7 (alpha.541). AC#3: Batch build 2.0.0-alpha.542+c00c5f0 (esp32-combo from origin/dev c00c5f025, 2026-10-30 00:47, exit 0, fresh artefacts, Discord msg 1565512587494301767); integration check on the merged set: evaluate --quick 100%, 0 failed; every tests/test_sat_*.py plus tests/test_manual_sat_commands.py exit 0. Fresh rerun this session on c00c5f025: python tests/test_sat_pid_integral_cap.py Ran 5, OK.

alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.

2026-10-30 close-out (session otgw-firmware-ea): blocker resolved. The alpha.542 bench failure (v2.js cut, settings loss) was fixed in later builds; alpha.547/548 OTA on Classic-S3 .63 kept 174 settings keys (coordinator b8 bench note on TASK-1418), and alpha.559+ccda6eb (posted, msg 1565776786720165938) contains b28f57ca7 (git merge-base --is-ancestor). Fresh evidence this session on src == origin/dev 8744d985a: python tests/test_sat_pid_integral_cap.py -> Ran 5 tests OK; python evaluate.py --quick -> Failed 0, Health 100%. Still not verified on a bench: a live run with the integral above 20 (needs a long cold-room run; the host test covers the arithmetic on the real SATpid.ino/SATcontrol.ino code).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the firmware-only +-20 integral caps (SATpid.ino _pidUpdateIntegral, SATcontrol.ino satZonePidAdvance). The integral is now bounded by the heating curve only, as thermo-nova pid.py:159-171, :185-192, helpers.py:148-150, area.py:208 (entry 75, Q25). The 10 s gate was already gone with TASK-1368. Merged b28f57ca7, alpha.541.

Evidence per AC:
- AC#1: tests/test_sat_pid_integral_cap.py red with origin/dev SATpid.ino + SATcontrol.ino (failures=4: primary 20.0 vs 22.857 and 40.0, zone 20.0 vs 33.9), green 5/5 on the fix; rerun 5/5 OK on c00c5f025 on 2026-10-30.
- AC#2: same test, two steps 5 s apart both move the integral (green on dev too, the TASK-1368 fix).
- AC#3: batch build 2.0.0-alpha.542+c00c5f0 exit 0 with fresh artefacts (Discord msg 1565512587494301767); evaluate --quick 100%, 0 failed on the merged set.
Not verified: a bench run with the PID integral above 20.
Status: In Progress. alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:FINAL_SUMMARY:END -->
