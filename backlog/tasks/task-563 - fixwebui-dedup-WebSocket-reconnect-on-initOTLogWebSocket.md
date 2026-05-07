---
id: TASK-563
title: 'fix(webui): dedup WebSocket reconnect on initOTLogWebSocket'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 17:48'
updated_date: '2026-05-07 18:21'
labels:
  - webui
  - websocket
  - frontend
  - 2.0.0
dependencies: []
priority: medium
ordinal: 26000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Frontend reconnect logic does not deduplicate against an in-flight handshake. SergeantD's browser console (alpha.3, 2026-05-07) shows initOTLogWebSocket() invoked from at least three call sites (updateOTLogResponsiveState @ 1541, initMainPage @ 3248, anonymous IIFE @ 1126) within the same second on page load — Connect attempts #1 and #2 fire within ~1 s of each other, and the attempt counter climbs to 30+ within ~4 minutes with most attempts failing code=1006. Independent of any server-side issue (TASK-529), this guarantees thrash and amplifies the visible 1006 storm. JS-only fix: refuse re-entry while readyState === CONNECTING || OPEN; cancel pending reconnect timer when a new init is requested only if the current socket is closed or errored. Cross-ref: TASK-529 server-side latency, TASK-431 WebUI freeze.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 initOTLogWebSocket() returns early with a debug log when otLogWS is non-null and readyState is CONNECTING (0) or OPEN (1); existing teardown-and-rebuild path runs only when readyState is CLOSING (2), CLOSED (3), or otLogWS is null
- [x] #2 force=true callers (performFlash at 6503; future explicit overrides) bypass the new guard so flash flow can deterministically replace an in-flight socket
- [x] #3 Pending reconnect timer is still cleared on entry (existing logic at 1584-1587 preserved); guard is placed AFTER the flashMode/displayState early returns and BEFORE the wsConnectionAttempts increment so the counter no longer climbs on suppressed re-entries
- [ ] #4 [FIELD] Verified by browser console capture: page load + 5 min idle on the OT log page produces at most 1 in-flight WS handshake at any time and connect-attempt counter increments only on actual disconnects
- [x] #5 No regression on the OT log live-stream behaviour; existing reconnect-on-disconnect timing (5 s backoff, 1 s during flash) preserved
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Confirmed via index.js read: page-load chain is window.onload->initMainPage()->updateOTLogResponsiveState() (line 3248) + 250ms-deferred scheduleOTLogWebSocketInit (line 3353 via showMainPage). Two attempts within ~1s.\n2. Existing guard at 1540 only protects the updateOTLogResponsiveState call site; scheduleOTLogWebSocketInit, firmwarePage (3359), and watchdog (1436) call initOTLogWebSocket directly with no state check.\n3. Fix: add early-return guard at the top of initOTLogWebSocket(force) that refuses re-entry while otLogWS.readyState === CONNECTING || OPEN. Bypass guard when force=true (flash flow deliberately stomps). Log the refusal.\n4. Verify build clean and evaluator green; commit on isolated branch with bump-hook disabled.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
---
**Plan reference**: implementation sequencing tracked in `/Users/Breee02/.claude/plans/clever-yawning-wreath.md` (local working plan, not in repo). **Ship 3** of the SergeantD-driven 2.0.0 fix sequence. **Re-scope required before coding**: data/index.js:1556 already has readyState gates at lines 1540, 1609, 1670, 1584-1587 — the remaining defect is more subtle. Read lines 530-700 + 1530-1700 in full; identify the precise call site that bypasses dedup OR the timer-vs-explicit-init race. Update ACs to match the actual gap.

Implementation complete on isolated worktree.

- Diagnosis: window.onload->initMainPage chain races two attempts within ~1s. initMainPage->updateOTLogResponsiveState (3248) opens attempt #1 immediately (otLogWS is null so the gate at 1540 admits). fetchDallasLabels().then->showMainPage->scheduleOTLogWebSocketInit(false, 250) at 3353 fires attempt #2 250ms later, tearing down the CONNECTING socket via the unconditional close-and-rebuild block at 1604-1615. firmwarePage (3359) and watchdog (1436) call initOTLogWebSocket() unguarded too.
- Fix: early-return guard inserted between displayState early returns and timer clears. Skips when otLogWS && readyState in {CONNECTING, OPEN} unless force=true. performFlash (6503) passes force=true, preserving deterministic socket replacement for flash flow. Reconnect-timer clear at 1584-1587 and onclose schedule at 1670/1674 untouched.
- Build: ./build.sh --firmware exit 0 (firmware + filesystem rebuilt for both ESP8266 and ESP32-S3).
- Evaluate: python3 evaluate.py --quick: 59 passed, 2 pre-existing warnings, 0 failed; health 97.1%.
- Commit: d1bf02b0 on isolated branch worktree-agent-a30ea64fcae167ffb (off f4fb97e). Bump hook disabled per task instructions; integration phase will bundle bump on the main 2.0.0 worktree.
- AC #4 [FIELD] left unchecked: requires browser console capture during 5-minute idle on hardware (SergeantD or similar).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added early-return guard at the top of initOTLogWebSocket() in src/OTGW-firmware/data/index.js to refuse re-entry while otLogWS is CONNECTING (0) or OPEN (1).

## What changed
- One block (16 lines) inserted between the displayState early-return (1581) and the existing reconnect-timer clear (1583), guarding callers that hit the function while a handshake is in flight or established.
- force=true (performFlash at 6503) bypasses the guard so the flash flow can deterministically replace the live socket; all other callers are now deduplicated.
- Existing logic untouched: reconnect timer clear (1584-1587), watchdog clear (1589-1592), connection counter increment (1599-1600), close-and-rebuild block (1604-1615), onopen/onclose/onerror/onmessage handlers, and onclose reconnect schedule (1670-1677).

## Why
SergeantD field log (alpha.3, 2026-05-07) showed Connect attempts #1 and #2 within ~1 s of page load and the counter climbing to 30+ in ~4 minutes with most attempts ending code=1006. Trace: window.onload -> initMainPage at 3112 -> updateOTLogResponsiveState at 3248 (opens attempt #1 because otLogWS starts null) -> fetchDallasLabels().then -> startMainPage -> showMainPage at 3331 -> scheduleOTLogWebSocketInit(false, 250) at 3353 -> initOTLogWebSocket(false) 250 ms later. The body unconditionally closed the in-flight CONNECTING socket (1604-1615) and opened a fresh one, doubling the attempt count and amplifying any server-side 1006 storm. Independent of TASK-529 (server-side latency) and TASK-431 (WebUI freeze).

## User impact
Fresh page load now opens exactly one socket and the connection-attempt counter only increments on real disconnect/reconnect cycles. Visible 1006 storm should drop to whatever the server-side rate is, with no client-side amplification. Flash flow unchanged (still uses force=true).

## Tests run
- Build: ./build.sh --firmware exit 0; ESP8266 + ESP32-S3 firmware and filesystem images rebuilt clean.
- Evaluate: python3 evaluate.py --quick: 59 passed, 0 failed, 2 pre-existing warnings (mqtt_configuratie.cpp not present, sendMQTTheapdiag arithmetic) unrelated to this change. Health 97.1%.

## Risk
Low. The guard refuses on a strict condition (CONNECTING || OPEN) and preserves all existing teardown paths verbatim. force=true bypass keeps flash flow behaviour identical. No new globals, no timing changes, no reconnect-backoff change.

## Follow-ups
- AC #4 [FIELD] requires browser-console capture during a real 5-minute idle session on hardware.
- Versioning: prerelease bump and push are intentionally deferred per task instructions; integration phase will bundle this fix with a single bump on the main 2.0.0 worktree.
<!-- SECTION:FINAL_SUMMARY:END -->
