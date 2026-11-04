---
id: TASK-955
title: >-
  Full v2 json-golden recapture from alpha.286 OTGW32 (complete the set TASK-944
  deferred)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 04:00'
updated_date: '2026-07-06 18:58'
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

REFRESHED 2026-07-06 (follow-up, task stays Done -- was already complete at alpha.286, this refreshes to current HEAD): recaptured all 31 v2 json-goldens from a freshly clean-flashed OTGW32 (192.168.88.39, MAC 10:20:BA:21:B4:F8, fw 2.0.0-alpha.333+d16ee2f, board Nodoshop OTGW32 (ESP32-S3), compiled Jul 6 2026, otdirectavailable=true, otcommandinterface=OT-Direct, psram_found=0). json_golden compare: 31 PASS / 0 FAIL, verified stable across 6 consecutive runs.\n\nFound and fixed TWO real bugs in scripts/json_golden.py (the capture tool itself, not the firmware) along the way: (1) fetch() caught http.client.IncompleteRead (and any other transient exception) into a bare 'ERR:...' string with NO retry, which for /v2/debug (6.3KB response) intermittently truncated mid-read against the freshly-booted device and baked a corrupted 'ERR:IncompleteRead(...){partial-then-full-json}' blob into the golden -- a live curl immediately after confirmed the endpoint itself was fine, confirming this was a capture-tool flakiness, not an endpoint bug. Added one retry. (2) internal_free/internal_maxblk (the PSRAM-aware internal-DRAM heap split added this session, TASK-978/994) on /v2/device/info and /v2/device/time were not in VOLATILE_EXACT, causing a flapping 1-2 FAIL across repeated compare runs purely from natural heap fluctuation -- added both to the volatile list, same treatment as the existing freeheap/maxfreeblock exact-match volatiles.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Full v2 json-golden recapture from alpha.286 OTGW32: all goldens current, json_golden compare 31 PASS / 0 FAIL (25/25 stable). Also fixed json_golden.py to strip the flat dotted-key /v2/debug runtime telemetry (runtime.heap*/uptime_sec/wifi_rssi) that could never be golden'd.
<!-- SECTION:FINAL_SUMMARY:END -->
