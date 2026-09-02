---
id: TASK-1108
title: >-
  PIC flash aborts when the WebSocket is unavailable instead of falling back to
  polling
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-02 20:42'
updated_date: '2026-09-02 20:44'
labels: []
dependencies: []
ordinal: 203000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
performFlash() in data/index.js (~line 5710-5731) waits up to 5s for otLogWS.readyState===1 and aborts with 'Error: Connection timed out. Cannot track progress.' when the WebSocket never opens. The fetch to /pic?action=upgrade is never sent, so no flash is attempted at all. This contradicts ADR-025, which states that flash operations rely entirely on HTTP polling. The failsafe poller (startFlashPolling on /api/v2/flash/status) is already started before the gate and works fine on its own. Reproduced on device OTGW 192.168.88.68 where port 81 refuses connections (see TASK-1107): update-check and download both succeed, only the flash never starts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The upgrade request is sent regardless of WebSocket state; a missing WebSocket degrades progress reporting to polling instead of blocking the flash
- [ ] #2 When the WebSocket is unavailable the UI says progress is tracked via polling, not that the operation failed
- [ ] #3 When the WebSocket is available it is still used for live progress, with polling as the failsafe
- [ ] #4 Flashing a hex file on a device with port 81 closed reaches 100% and reports completion
<!-- AC:END -->
