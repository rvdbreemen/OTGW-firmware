---
# METADATA
Document Title: Browser Compatibility Action Checklist
Review Date: 2026-01-26 00:00:00 UTC
Branch Reviewed: dev → dev (merge commit N/A)
Target Version: v1.0.0-rc4+
Reviewer: GitHub Copilot
Document Type: Action Checklist
PR Branch: dev
Commit: N/A
Status: COMPLETE
---

# Browser Compatibility Action Checklist

## Verification Steps

- [ ] Verify Fetch API uses `response.ok` checks for all WebUI requests.
- [ ] Confirm JSON parsing is wrapped in try-catch for all incoming data paths.
- [ ] Validate WebSocket keepalive reception every ~30 seconds.
- [ ] Confirm WebSocket watchdog timeout is 45 seconds or greater.
- [ ] Confirm progress bar updates use `textContent` instead of `innerText`.
- [ ] Confirm CSS transition rules include standard `transition` (vendor prefix optional).

## Manual Browser Tests

- [ ] Chrome (latest): Open WebUI, leave for 60+ minutes, confirm no stalls.
- [ ] Firefox (latest): Open WebUI, leave for 60+ minutes, confirm no stalls.
- [ ] Safari (latest + 2 back): Open WebUI, leave for 30+ minutes, confirm no stalls.
- [ ] Edge (latest): Verify WebUI behavior matches Chrome.

## Network Edge Cases

- [ ] Simulate port 81 drop and confirm auto-reconnect.
- [ ] Reboot ESP8266 and confirm WebSocket reconnects without reload.
- [ ] Test with missing `content-type` header for flash endpoint.

## Status

- Current documentation indicates fixes are implemented; this checklist is for validation and regression testing.
