---
id: TASK-3046
title: >-
  Retire diagRec in 2.0.0: the telemetry recorder's local ring serves
  /api/v2/diag/recording (ADR-268)
status: To Do
assignee: []
created_date: '2026-11-25T05:40:00Z'
updated_date: '2026-11-27T20:05:00Z'
labels:
  - tooling
  - diag
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1152000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-268 (Accepted by Robert 2026-11-25, supersedes ADR-213 and ADR-214): diagRec is removed in full in 2.0.0, in the same change that brings the remote telemetry recorder's local ring (RecRing.h, OTGWMIX1 format; PSRAM 1.5 MB, 8 KB heap without PSRAM) into the normal build. /api/v2/diag/recording* keeps its paths and status keys and is served from the new ring. Today diagRec is referenced in MQTTstuff.ino (:847, :2070, :2127), OTDirect.ino (:1110-1112, :3997), OTGW-Core.ino (:4593, :6130, :6473) and OTGW-firmware.h (:40-42, diagRecorderStuff.h). Readers: scripts/otgw_capture.py (TASK-1964) and scripts/diag_decode.py. Size gate context: TASK-1993, TASK-2980 (A1 99.5 % until 2026-11-29).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every diagRec reference is gone from src/OTGW-firmware (MQTTstuff.ino, OTDirect.ino, OTGW-Core.ino, OTGW-firmware.h, diagRecorderStuff.h): git grep -i diagrec on src/OTGW-firmware returns nothing
- [ ] #2 /api/v2/diag/recording* keeps its paths and status keys and serves the telemetry recorder's local ring: a curl of start, status, download and stop on the bench returns a valid recording that scripts/otgw_capture.py downloads and decodes
- [ ] #3 Bench on .61 (no PSRAM), BEFORE the swap lands on dev: one tester capture with the recorder shows the 8 KB heap ring holds a useful window (minutes of records); the measured window is recorded in the task
- [ ] #4 Flash saving measured: app image size before and after on the esp32-combo build, both numbers and the delta in the task notes
- [ ] #5 Host tests for the recorder path and the REST endpoints are red on the old code where they test the new ring and green on the change; python scripts/run_tests.py and python evaluate.py --quick show no new FAIL
- [ ] #6 The capture tool and decoder (scripts/otgw_capture.py, scripts/diag_decode.py) read the new format, or the task records why no change is needed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
otgw-architect verification 2026-11-27 21:xx (D-class triage): proposed as a duplicate of TASK-3132 AC4 (diagRec removed in the same change as the telemetry merge, ADR-268). Not closed: the coordinator decides.
<!-- SECTION:NOTES:END -->
