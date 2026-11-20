---
id: TASK-2008
title: >-
  SAT ground truth: a test that the local thermo-nova copy matches the pin
  7beaaf5
status: Done
assignee:
  - '@claude'
created_date: '2026-11-16T19:51:00Z'
updated_date: '2026-11-16T19:56:00Z'
labels:
  - sat
  - tests
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1028000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT ground-truth rule says the local thermo-nova copy (other-projects/SAT-releases-thermo-nova) is identical to the pin 7beaaf5 and is never updated silently, but nothing checked it. On 2026-11-16 the coordinator and ba read the other-projects commit c5211afe (git log -1 in that folder) as the thermo-nova version and suspected drift. A sha256 manifest of the pinned tarball plus a test makes the claim checkable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Manifest tests/data/thermo_nova_7beaaf5.sha256 of every file of the GitHub tarball at 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb
- [x] #2 tests/test_thermo_nova_pin.py passes on the local copy, fails on a changed, missing or extra file, and skips without other-projects
- [x] #3 Finding recorded: the local copy is byte-identical to the pin today; c5211afe is the other-projects commit that adds UPSTREAM_COMMIT
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 (95, coordinator decision: no re-pin, test only). Evidence: tarball https://codeload.github.com/Alexwijn/SAT/tar.gz/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb (293,230 B, 127 files) vs other-projects/SAT-releases-thermo-nova: diff -rq identical (also without CR stripping), the only extra local file is UPSTREAM_COMMIT ('7beaaf531ee9... releases/thermo-nova 2026-03-18 verified identical 2026-10-22'). The folder's history in OTGW-other-projects: 90cd6179 initial snapshot, c5211afe 'Record the upstream commit the thermo-nova snapshot mirrors (7beaaf5)' (adds UPSTREAM_COMMIT only); 770aa45b (OT-Thing refresh, 11 Nov) touches no path in it. So nothing changed upstream in the copy and nothing was restored. Test runs: real copy 2/2 OK; a doctored temp copy (cycles/const.py OVERSHOOT_MARGIN 3.0 -> 1.5, README.md removed, extra.txt added) FAIL with changed [custom_components/sat/cycles/const.py], missing [README.md], extra [extra.txt]; OTGW_THERMO_NOVA_DIR pointing nowhere: skipped. Allowed extras: UPSTREAM_COMMIT and Python caches (the 2026-10-22 check also saw only __pycache__).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The local thermo-nova copy is now checked against the pin. AC1: tests/data/thermo_nova_7beaaf5.sha256, 127 sha256 lines from the GitHub tarball at 7beaaf5. AC2: tests/test_thermo_nova_pin.py: OK on other-projects/SAT-releases-thermo-nova; FAIL on a doctored copy (one changed, one missing, one extra file, each named); skip without the folder (CI has no other-projects). AC3: the copy is byte-identical to the pin (diff -rq vs the tarball); c5211afe is the OTGW-other-projects commit that adds UPSTREAM_COMMIT, not a thermo-nova version; nothing restored, no submodule or pointer change. Not done: wiring the test into evaluate.py (run_tests.py picks it up as a tests/test_*.py file).
<!-- SECTION:FINAL_SUMMARY:END -->
