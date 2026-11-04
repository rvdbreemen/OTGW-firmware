---
id: TASK-927
title: >-
  Combine dev + 1.x capture-mqtt-debug.bat into one unified script (full
  function union)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-24 21:02'
updated_date: '2026-06-24 21:26'
labels: []
dependencies: []
ordinal: 142000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
dev's capture-mqtt-debug.bat predates the crashlog-poll feature (only has a one-shot curl Invoke-HttpProbes sweep). Port the continuous crashlog/reboot_log poller from the otgw-1.x.x script, including the TASK-926 resilience hardening (10s timeout, one retry, classified WebException), keeping dev's existing lineage and its curl probe sweep intact.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Crashlog worker (CrashlogWorkerScript + Start/Stop-CrashlogCapture) present in dev script
- [x] #2 Resilient poller (10s timeout, 1 retry, classified errors) included, matching TASK-926
- [x] #3 crashlog.log wired into merged transcript and file list
- [x] #4 Params + help text for crashlog poll added; dev curl Invoke-HttpProbes sweep retained
- [x] #5 PS payload parses with 0 errors
- [x] #6 All 1.x-only functions added to dev script (crashlog worker x3, metadata x4, tool-error x2, prefill x3, ConvertTo-SafeFileNamePart)
- [x] #7 dev-only Invoke-HttpProbes curl-sweep retained with its params, help, orchestration and curl-probes.log wiring
- [x] #8 dev -SaveSecrets autonomous-secret subsystem preserved (needed by otgw-test.py); Save-CaptureSettings is the secure-by-default superset
- [x] #9 Function set of merged == union(devorig, 1.x): no function missing, none duplicated
- [ ] #10 PS payload parses 0 errors and bat --help runs end-to-end showing all three feature sections
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Merged dev and otgw-1.x.x capture-mqtt-debug.bat into one unified script on dev, using 1.x as base (firmware-agnostic, already handles 2.0.0/OTGW32 toggles+banner, carries the TASK-926 resilient crashlog poller) and grafting back dev's two unique features: the Invoke-HttpProbes post-capture curl sweep (with 2.0.0 /api/v2/sat/status + /api/v2/flash/status probe targets) and the -SaveSecrets autonomous-secret subsystem used by otgw-test.py. Verified deterministically: merged function set == union(devorig 44, 1.x 55) = 56, nothing missing/duplicated; top-level diff vs 1.x is exactly the two grafted features and nothing else; .bat launcher header identical between lineages; PS payload parses 0 errors; bat --help runs end-to-end and shows Browser/Crash-log/HTTP-probes sections. Save-CaptureSettings kept dev's body because it is the secure-by-default superset (writes the MQTT secret only with opt-in -SaveSecrets to an out-of-repo store).
<!-- SECTION:FINAL_SUMMARY:END -->
