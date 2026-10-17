---
id: TASK-1235
title: >-
  Investigate: v2 WebSocket fails with 'Invalid frame header' on Monitor page
  load (Classic-S3)
status: To Do
assignee: []
created_date: '2026-10-15 11:20'
updated_date: '2026-10-17 09:25'
labels:
  - bug
  - webui
  - websocket
milestone: m-2
dependencies: []
priority: medium
ordinal: 373000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen on the Classic-S3 bench (.88.63, alpha.423, v2-only image, 2026-10-15): in 3 of 3 Monitor-page loads Chrome logged 'WebSocket connection to ws://192.168.88.63/ws failed: Invalid frame header' (once with a 503), then reconnected. The PS=1 chip detection in v2 only runs while the socket is open, so one run missed it for 25 s. Not seen on the OTGW32 run the same day. Unknown whether it predates the classic removal. Evidence: .tmp/bench-classic/after/ui_e.json, ui_e2.json.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced with a capture (capture-mqtt-debug or browser devtools capture) on the old and the current firmware, so it is known whether it is a regression
- [ ] #2 Root cause named with evidence (frame bytes or server-side log)
- [ ] #3 Fixed or explicitly accepted with a reason; if fixed, old-vs-fix capture shows the error gone
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-17 rerun on the Classic-S3, alpha.427+543dc4e (LittleFS from a74a23a): 5 fresh Monitor loads, 'Invalid frame header' 0 of 5, every load 1 socket with handshake 101 and valid Sec-WebSocket-Accept, first message after 1.7-3.4 s, 0 reconnects. On alpha.423 it was 3 of 3. Not reproduced, cause unknown: firmware changed between the runs, and this was a single sequential client. Evidence .tmp/bench-classic/after427/ui_I.json. Keep open until it is seen again or a cause is found.
<!-- SECTION:NOTES:END -->
