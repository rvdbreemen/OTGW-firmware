---
id: TASK-3009
title: >-
  Capture tool: reconnect a device stream that delivers nothing for N min while
  the health probe says the device is fine
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T14:56:00Z'
updated_date: '2026-11-27T05:21:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1130000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-3006 (otgw-dev analysis, will_85903 20 Nov): 08:04-13:18 both device streams (telnet, /ws frames) delivered nothing with open sockets and a healthy device; the 60 s telnet idle check only reconnects when the HTTP health probe fails or the uptime drops, so the tool kept waiting. TASK-3006 now REPORTS such a silence (StreamWatch); this task makes the tool also reconnect that stream and mark the gap.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A telnet or frame stream silent for N min while the health probe is OK is reconnected once, with a gap marker
- [x] #2 A test on the real tool (fake device that keeps the socket open but sends nothing) fails on the old code and passes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: deferred to 2026-11-25 (token budget, Robert via coordinator). Commits stay as they are, no builds, no new work until then. The .63 25 h soak of 464a188 runs unattended; 09 reads only the end result. The capture e2e runs by 09 on .61 may finish; their results get read on 25 Nov.

Fix in scripts/otgw_capture.py: TelnetCapture.silent_reconnect and FrameCapture.silent_reconnect (seconds, set from --stream-silent-minutes via silent_reconnect_seconds(args); 0 = off). A telnet stream with no byte, or a /ws stream with no frame, for N while the HTTP health probe answers (any HTTP status) is reconnected once per silence; the gap starts at the last byte or frame and is marked in telnet.log, otframes.log, gaps.log and summary.txt (reason 'stream silent N, health probe OK'). A failing probe leaves the case to the existing idle check.

Evidence: tests/test_capture_stream_silence.py (real TelnetCapture and FrameCapture, FakeDevice and a /ws fake that hold the socket open and send nothing). RED on the old code: a (accepts=1, never reconnected), c (connections=1), e (no silent_reconnect_seconds); b and d (probe failing, no reconnect) pass as guards. GREEN with the fix: 5 tests OK, 3 reruns OK. Whole capture test set green (test_capture_tool 63 tests OK, 2 skipped; the other 12 capture files OK); evaluate.py --quick: 0 failed, 0 warnings. Not verified: a real device, the multi-hour case; the N in the tests is 2 s, the tool default stays 10 min.
<!-- SECTION:NOTES:END -->
