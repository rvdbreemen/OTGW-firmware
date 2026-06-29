---
id: TASK-944
title: Refresh v2 SAT json-golden set against a clean alpha.284+ OTGW32
status: To Do
assignee: []
created_date: '2026-06-28 21:48'
labels:
  - async-esp32s3
dependencies: []
ordinal: 157000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The scripts/json-golden set (v2_sat_status.json, v2_health.json) is ~70 alpha versions stale (captured alpha.210, 2026-06-18). json_golden.py compare FAILs broadly from build version/githash drift, network/config drift, and accumulated otd*/ADR-155 health fields (incl. thermostat_age_s/boiler_age_s added alpha.276) — not from any single feature. Split out of TASK-925 AC#4 per that task's own recommendation: a blind full recapture on a mismatched device would bake in idle + fw/fs-mismatch + device-specific network state. Capture against a CLEAN alpha.284+ device with matching fs+config, with SAT active + a source present.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 json-golden v2_sat_status.json + v2_health.json regenerated from a clean alpha.284+ OTGW32 (fw/fs matched, SAT active, source present)
- [ ] #2 json_golden.py compare passes (or documents each intentional residual diff)
- [ ] #3 captured device state documented (version, githash, network, settings) so the next drift is attributable
<!-- AC:END -->
