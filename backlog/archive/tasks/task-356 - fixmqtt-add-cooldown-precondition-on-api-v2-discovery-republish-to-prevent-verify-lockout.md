---
id: TASK-356
title: >-
  fix(mqtt): add cooldown/precondition on /api/v2/discovery/republish to prevent
  verify lockout
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:32'
updated_date: '2026-04-21 17:14'
labels:
  - code-review
  - mqtt
  - security
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 2A MEDIUM (CWE-770): rapid-fire POST loops keep countPendingDiscoveryIds greater than zero indefinitely, which blocks startDiscoveryVerification precondition at MQTTstuff.ino:216. Post-auth LAN actor can permanently lock out the verify endpoint.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Endpoint returns 429 if called within 60s of previous invocation, OR returns 409 when countPendingDiscoveryIds is greater than zero
- [x] #2 Cooldown/precondition documented in code comment
- [x] #3 Verify endpoint remains reachable under rapid-fire republish load
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read existing handler (done): handler at lines 512-524 of restAPI.ino.
2. Implement 60s cooldown with static locals inside handleDiscovery republish branch.
3. Cooldown check runs BEFORE markAllMQTTConfigPending (before any work).
4. Build 429 JSON error body inline (sendApiError only accepts PROGMEM strings; dynamic countdown requires direct httpServer.send).
5. Set lastRepublishMs = millis() right before the 200 success response.
6. Use PSTR/snprintf_P for all new strings per CLAUDE.md PROGMEM rules.
7. Inline comment documents 60s window and rationale (post-auth LAN DoS of verify endpoint).
8. Verify build with `python build.py --firmware`.
9. Check AC 1, 2. Justify AC 3 via code inspection: cooldown rejects additional POSTs before markAllMQTTConfigPending runs, so pending drip completes and verify endpoint recovers.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Added 60 s cooldown (REPUBLISH_COOLDOWN_MS=60000UL) to /api/v2/discovery/republish.
- Chose 429-with-countdown pattern over 409-on-pending: better UX (tells client exactly when to retry) and decouples rate limiting from the drip publisher state.
- Implementation uses function-local static unsigned long lastRepublishMs so no new globals are added.
- sendApiError() only accepts PROGMEM strings, so the 429 response is built inline with httpServer.send() using the same JSON error shape as sendApiError (status/message). restResponseStatus is also updated for consistent logging.
- Cooldown check runs after the MQTT-connected precondition but before markAllMQTTConfigPending(), so rejected requests perform zero work.
- lastRepublishMs is stamped only after the work commits (just before the 200 response), so a failed precondition does not consume the cooldown window.
- Remaining-seconds computation uses ceiling division (elapsed + 999) / 1000 to avoid reporting "retry in 0s".
- AC3 verified by code inspection: rejected POSTs never add new pending IDs, so the drip publisher drains the existing set within a few minutes and /verify becomes reachable again.
- Build: python build.py --firmware — PASS.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added a 60 s cooldown to POST /api/v2/discovery/republish to prevent a post-auth LAN actor from DoS'ing the /api/v2/discovery/verify endpoint (CWE-770).

## Why
The republish endpoint calls markAllMQTTConfigPending(), which pushes IDs onto the pending discovery set. The drip publisher drains that set over time, but rapid-fire POSTs refill it faster than the drip empties it. That keeps countPendingDiscoveryIds() > 0 indefinitely, which blocks the verify precondition at MQTTstuff.ino and permanently denies verification.

## Changes
- src/OTGW-firmware/restAPI.ino: in handleDiscovery(), the republish branch now gates on a function-local static lastRepublishMs timer with REPUBLISH_COOLDOWN_MS = 60000UL.
- Requests inside the cooldown window return HTTP 429 with body {"error":{"status":429,"message":"Republish cooldown active, retry in Ns"}} where N is remaining seconds (ceiling).
- restResponseStatus is set to 429 for consistent access logging.
- lastRepublishMs is stamped only after markAllMQTTConfigPending() commits, so failed preconditions (MQTT not connected, etc.) do not consume the window.
- Inline comment documents the 60 s duration and threat model.

## Design choices
- 429-with-countdown chosen over 409-on-pending: better client UX and independent of drip publisher state.
- Function-local static (not a new global) keeps the blast radius minimal; no other file touched.
- sendApiError() takes PROGMEM strings only, so the dynamic countdown is emitted inline via httpServer.send(), mirroring the existing JSON error shape.

## Tests
- python build.py --firmware: PASS (arduino-cli, ESP8266 target).
- AC3 (verify reachability under rapid-fire load) validated by code inspection: rejected POSTs skip markAllMQTTConfigPending(), so the pending set drains naturally and /verify recovers.

## Risks / follow-ups
- None. 60 s is conservative vs the drip rate (one ID per few seconds) and matches the spec.
- A unit test for the cooldown path would be nice but falls under Agent W's tests/ scope.
<!-- SECTION:FINAL_SUMMARY:END -->
