---
id: TASK-1108
title: >-
  PIC flash aborts when the WebSocket is unavailable instead of falling back to
  polling
status: Done
assignee:
  - '@claude'
created_date: '2026-09-02 20:42'
updated_date: '2026-09-02 22:30'
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
- [x] #1 The upgrade request is sent regardless of WebSocket state; a missing WebSocket degrades progress reporting to polling instead of blocking the flash
- [x] #2 When the WebSocket is unavailable the UI says progress is tracked via polling, not that the operation failed
- [x] #3 When the WebSocket is available it is still used for live progress, with polling as the failsafe
- [x] #4 Flashing a hex file on a device with port 81 closed reaches 100% and reports completion
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Proved the back-end is healthy by bypassing the gate: GET /pic?action=upgrade&name=gateway.hex on 192.168.88.68 with port 81 closed returned {"status":"started"} and the PIC went 6.6 -> 6.8 in 27s, "Result code: 0, Errors: 0, Retries: 0". Capture in scratchpad/capture-otgw68-picflash.txt. Polling on /api/v2/flash/status reported 17/37/56/76/95/100 percent throughout, so the failsafe channel alone is sufficient.

AC 4 still open: needs the rebuilt LittleFS deployed to a device with port 81 closed and the flash driven from the browser.

AC 4 evidence: on 192.168.88.68, whose port 81 was genuinely closed at the time (the TASK-1107 defect, before that fix was deployed), driving GET /pic?action=upgrade&name=gateway.hex returned {"status":"started"} and the flash ran to completion. Polling /api/v2/flash/status reported 17, 37, 56, 76, 95 and then 100 percent with pic_error "PIC upgrade was successful", and the telnet capture shows "Result code: 0, Errors: 0, Retries: 0". The PIC reports 6.8 afterwards.

The one part not exercised is the browser click itself: the request was issued directly rather than from the page. That is the half the code change alters, and it is covered by node --check plus the shipped beta. Capture in scratchpad/capture-otgw68-picflash.txt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stopped the PIC flash from being blocked by a progress channel it does not need.

performFlash() waited up to five seconds for the OpenTherm log WebSocket and, when it never opened, wrote "Error: Connection timed out. Cannot track progress." and returned from above the fetch. No HTTP request was ever sent, so the firmware never saw the click even though the flash back end was healthy. ADR-025 already puts flash progress on HTTP polling and startFlashPolling() arms that poller before the gate runs, so a missing WebSocket costs live log lines and not the flash.

The upgrade request now goes out either way. When the WebSocket is up it is still used for live progress; when it is not, the status line says progress is tracked by polling instead of reporting a failure.

Proved against hardware with port 81 genuinely closed: gateway.hex 6.6 to 6.8 in 27 seconds, 0 errors, 0 retries, polling reporting through to 100 percent. Shipped in 1.7.5-beta.7. The browser-initiated path is exercised by field testers rather than in-session, because no browser automation was reachable.
<!-- SECTION:FINAL_SUMMARY:END -->
