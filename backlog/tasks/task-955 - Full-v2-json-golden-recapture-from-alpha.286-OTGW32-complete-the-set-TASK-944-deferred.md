---
id: TASK-955
title: >-
  Full v2 json-golden recapture from alpha.286 OTGW32 (complete the set TASK-944
  deferred)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 04:00'
updated_date: '2026-06-30 04:11'
labels: []
dependencies: []
ordinal: 168000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-944 (which refreshed only v2_sat_status + v2_health). 7 goldens remain alpha.210-stale: debug, settings, device_info, filesystem/files, discovery, sat, sat/status?detail=full. Recapture the FULL set from the clean bench OTGW32 @192.168.88.39 (alpha.286+45005ff) so json_golden compare is fully green. Note: settings/device_info goldens bake in the bench device network (ssid/broker/masked-pw-length/MAC) — acceptable as the canonical baseline per maintainer; passwords are API-masked, not real secrets.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Full json_golden capture from .39 (alpha.286) regenerates all goldens
- [x] #2 json_golden.py compare .39 reports 0 semantic FAIL (byte-only diffs from volatile fields are OK)
- [x] #3 Captured device state documented (version/githash/board/compiled)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
DONE 2026-06-30: full json_golden capture from .39 (alpha.286+45005ff, Nodoshop OTGW32 ESP32-S3, compiled Jun 29) -> all goldens regenerated (13 changed). Fixed a real json_golden.py gap: /v2/debug uses FLAT dotted-string keys ('runtime.heap_free', not nested), so the heap/uptime/rssi telemetry was never volatile-stripped and could never pass. Added 'runtime.heap' to VOLATILE_PREFIX + 'runtime.uptime_sec'/'runtime.wifi_rssi' to VOLATILE_EXACT. Result: compare = 31 PASS / 0 FAIL, verified 25/25 consecutive clean runs. NOTE: a rare flap (~1/5) appeared while the device was still settling right after the 30-min soak; candidate fields if it ever recurs in CI = the state.heap.* / state.disco.* activity counters (add to volatile then). On a quiesced device it is stable.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Full v2 json-golden recapture from alpha.286 OTGW32: all goldens current, json_golden compare 31 PASS / 0 FAIL (25/25 stable). Also fixed json_golden.py to strip the flat dotted-key /v2/debug runtime telemetry (runtime.heap*/uptime_sec/wifi_rssi) that could never be golden'd.
<!-- SECTION:FINAL_SUMMARY:END -->
