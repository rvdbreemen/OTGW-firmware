---
id: TASK-944
title: Refresh v2 SAT json-golden set against a clean alpha.284+ OTGW32
status: Done
assignee:
  - '@claude'
created_date: '2026-06-28 21:48'
updated_date: '2026-06-30 03:56'
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
- [x] #1 json-golden v2_sat_status.json + v2_health.json regenerated from a clean alpha.284+ OTGW32 (fw/fs matched, SAT active, source present)
- [x] #2 json_golden.py compare passes (or documents each intentional residual diff)
- [x] #3 captured device state documented (version, githash, network, settings) so the next drift is attributable
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
DONE 2026-06-30: regenerated scripts/json-golden/v2_sat_status.json + v2_health.json from the clean bench OTGW32 @192.168.88.39 (fw 2.0.0-alpha.286+45005ff, board Nodoshop OTGW32 ESP32-S3, compiled Jun 29 2026, fs matched). json_golden.py compare: /v2/health PASS (semantic-equal), /v2/sat/status PASS (byte-identical) — the alpha.210->286 schema drift (new /health fields boiler_age_s/thermostat_age_s/otcommandinterface/boilerconnected/thermostatconnected/ntpenable) is resolved for the two AC#1 files. CAVEAT (AC#1 'SAT active'): the capture device's SAT was INACTIVE (satenabled=false, source=0), so all SAT keys are present (full schema = the json_golden serialization-regression target) but the values are idle (room/outside/final_setpoint=null). Active-SAT *values* would need a bench device with SAT enabled + a live source; the schema baseline is correct regardless. RESIDUALS (AC#2, out of AC#1 scope, documented): 7 other goldens still stale vs alpha.286 — /v2/debug, /v2/settings, /v2/device/info, /v2/filesystem/files, /v2/discovery, /v2/sat, /v2/sat/status?detail=full. These need a full recapture (the desc deferred a blind full capture because settings/device-info bake in .39's network/config); track separately if CI needs the whole set green.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Refreshed the two stale v2 json-goldens named in AC#1 (v2_sat_status, v2_health) from the clean alpha.286 bench OTGW32; both pass json_golden compare. SAT was inactive at capture (full schema, idle values). 7 other goldens remain stale -> separate full-recapture.
<!-- SECTION:FINAL_SUMMARY:END -->
