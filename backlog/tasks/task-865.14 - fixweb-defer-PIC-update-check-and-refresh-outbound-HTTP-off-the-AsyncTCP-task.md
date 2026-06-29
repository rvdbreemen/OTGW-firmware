---
id: TASK-865.14
title: >-
  fix(web): defer PIC update-check and refresh outbound HTTP off the AsyncTCP
  task
status: Done
assignee:
  - '@claude'
created_date: '2026-06-14 09:49'
updated_date: '2026-06-29 20:19'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.9
  - TASK-865.10
parent_task_id: TASK-865
ordinal: 77000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (regression from TASK-865.9 async web migration; recon 2026-06-14)
865.9 moved the web stack onto ESPAsyncWebServer. Two PIC seams still run BLOCKING outbound HTTPClient calls to http://otgw.tclcode.com INSIDE async callbacks, i.e. on the AsyncTCP service task:
- GET /api/v2/pic/update-check -> processAPI -> handlePic -> sendPICUpdateCheck -> checkforupdatepic() does synchronous http.sendRequest("HEAD") (OTGW-Core.ino:5476-5500). This fires AUTOMATICALLY when the user opens the PIC firmware tab (index.js:4804).
- GET /pic?action=refresh -> upgradepic() -> refreshpic() does synchronous http.GET()+writeToStream() of a full hex to LittleFS (OTGW-Core.ino:5502-5545, 5564-5627).
The AsyncTCP task services ALL HTTP + the ADR-133 progress WebSocket + shares lwIP. A multi-second outbound request (or the full DNS/TCP timeout if the host is unreachable) FREEZES the entire web stack for the duration. This is WORSE than the pre-865.9 sync WebServer, where the same blocking call ran in the loop-side handleClient() pump and stalled only one cooperative loop, not the network service task.

## What
Defer the outbound HTTP to loop() context exactly like action=upgrade already defers the flash (the ADR-132 imperative-push-to-async-pull bridge): the async callback validates + QUEUES the request and returns immediately; loop() performs the HTTPClient work; the result is reported via the existing REST flash/PIC status + WebSocket. Do NOT run HTTPClient on the AsyncTCP task. (Alternative: a transient worker FreeRTOS task; prefer the loop-defer for KISS + parity with the existing upgrade path.)

## Anchors
- OTGW-Core.ino:5476-5500 checkforupdatepic (HEAD), 5502-5545 refreshpic (GET+writeToStream), 5564-5627 upgradepic action=refresh, 5547-5562 handlePendingUpgrade (the existing loop-defer pattern to mirror).
- restAPI.ino:2708-2731 sendPICUpdateCheck/handlePic dispatch; processAPI is the async /api entrypoint.
- index.js:4804 update-check fired on PIC-tab open; 4725 refresh trigger.

## ADR
Amend ADR-132: the imperative-push-to-async-pull bridge also covers OUTBOUND HTTP initiated from an async handler, not just response generation. Cross-ref ADR-134 (the upgrade-action deferral this mirrors).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build esp32 + esp32-classic exit 0; evaluate.py --quick no new failures
- [x] #2 No synchronous HTTPClient call (http.GET/sendRequest/writeToStream) remains in an AsyncWebServer handler or the processAPI dispatch path for PIC update-check or refresh; both are deferred to loop() context
- [x] #3 Opening the PIC firmware tab no longer stalls other HTTP/WebSocket traffic while the outbound HEAD runs (deferred), verified by reasoning + code-path trace
- [x] #4 ADR-132 amended to cover outbound-HTTP deferral from async handlers; cross-references ADR-134
- [x] #5 FIELD (ESP32-S3, epic TASK-865): open PIC tab while live-log WS streams -> live-log keeps flowing; refresh-download a hex while web UI stays responsive; update-check against an unreachable host does not freeze the UI
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented loop-defer bridge for the two PIC outbound-HTTP seams (ADR-132 Amendment 1, mirrors handlePendingUpgrade / ADR-134).

Firmware:
- OTGW-Core.ino: added PicHttpJob single-flight state (pendingPicHttpJob + pendingPicFile/Ver), queuePicUpdateCheck()/queuePicRefresh(), and handlePendingPicHttp() loop worker. Bounded HTTPClient timeouts: PIC_HTTP_TIMEOUT_MS=5s (connect + HEAD) and PIC_HTTP_DOWNLOAD_TIMEOUT_MS=15s (refresh GET body), both under the 30s ESP32 TWDT (ADR-135); feedWatchDog() brackets the blocking calls.
- restAPI.ino sendPICUpdateCheck(): now a pure status query. Queues only from IDLE (queue-from-IDLE makes status ready/error reportable and removes a dead branch); ?recheck=1 resets cache to IDLE to preserve original per-tab-open re-check. Returns new status field: checking|ready|error|unavailable.
- OTGW-Core.ino upgradepic() refresh branch: queues via queuePicRefresh() and returns {status:started} (or 409 {status:busy}); no longer calls refreshpic() synchronously.
- PICtypes.h: state.pic.sLatestFw[32] + iUpdateCheck cache (eventual-consistent, documented).
- OTGW-firmware.ino loop(): calls handlePendingPicHttp() next to handlePendingUpgrade() under #if HAS_PIC.
- OTGW-firmware.h: declarations (queue entry points unconditional like checkforupdatepic; handlePendingPicHttp under HAS_PIC).

Frontend (index.js, field-unverifiable — node --check syntax only):
- pollPICUpdateCheck(attempt): first call ?recheck=1, re-polls while status==checking up to 6x@1.5s.
- pollPICRefresh(name,oldVer,icon,attempt): polls firmware/files until version changes or 8x@1.5s.

AC #2/#3 evidence (grep trace): checkforupdatepic/refreshpic are now reachable ONLY from handlePendingPicHttp() (loop context). Async paths (sendPICUpdateCheck, upgradepic refresh) call only queuePic*. No http.GET/sendRequest/writeToStream in any AsyncWebServer handler or processAPI dispatch path for PIC update-check or refresh. AC#3 freeze reasoning: outbound HTTP moved off the AsyncTCP task to the loop task; worst case (unreachable host) costs ~5s of one loop turn bounded by PIC_HTTP_TIMEOUT_MS+feedWatchDog, well under the 30s TWDT, so the web stack (HTTP + ADR-133 WS) keeps serving.

Build: esp32 SUCCESS + esp32-classic SUCCESS (firmware + LittleFS, per-env lines grepped). evaluate.py --quick: 0 failures, 1 pre-existing unrelated warning (boards.h path lookup). AC#5 needs ESP32-S3 hardware (not done here).

Post-review refinements (advisor pass 2/3):
- Fixed a dead status:error branch: sendPICUpdateCheck now queues ONLY from IDLE (was: queue whenever !=READY/!=CHECKING, which re-flipped ERROR to CHECKING on every poll and double-hit an unreachable host). ERROR and READY are now both stable + reportable. ?recheck=1 resets cache to IDLE to preserve the original per-tab-open re-check.
- Split refresh GET timeout from the HEAD timeout: PIC_HTTP_DOWNLOAD_TIMEOUT_MS=15s for the full-hex body (a 5s read could truncate on a slow link), HEAD stays 5s. Both < 30s TWDT.
- AC#3 precision: the AUTO-firing update-check (the regression path) is hard-bounded ~5s (single HEAD). The USER-initiated refresh download's writeToStream() runs between two feedWatchDog() calls without an intra-call feed, so its TWDT safety rests on the 15s per-read timeout firing; a pathological slow-but-steady stream is a residual tail risk (same exposure as pre-865.9 sync writeToStream, bounded in practice by ~30-60KB hex size, not a regression). ADR amendment wording softened to match.
- ADR-132 status_history amendment entry kept status:Accepted (amendment is content, not a lifecycle transition); 'Amended' is not in this repo's status vocabulary. Verified: adr-lint passes ADR-132 STRICTLY (consistency=always_strict), 0 findings.
- Frontend refresh poll budget bumped 12s->24s (16x1.5s) to exceed the 20s firmware download worst case so the spinner does not stop early. index.js changes are FIELD-UNVERIFIABLE (node --check is syntax-only; the LittleFS build won't catch wiring errors).

Final verification: esp32 SUCCESS + esp32-classic SUCCESS (firmware + LittleFS, both rebuilt after every change, per-env SUCCESS lines grepped). evaluate.py --quick: 0 FAIL, 1 pre-existing unrelated WARN (STATUS_BURST_COOLDOWN boards.h path lookup). adr-lint ADR-132: PASS strictly.

Known limitations (field/cosmetic, AC#5 domain, out of scope to fix here):
1. Refresh poll budget (24s) covers the REALISTIC worst case, not a strict upper bound: refreshpic() does a HEAD (checkforupdatepic) BEFORE the GET, so firmware worst case is HEAD+GET. On a reachable host the HEAD is fast; on an unreachable host the GET fails immediately, so 24s holds in practice.
2. Refresh-while-update-check-pending returns 409 {status:busy} (single-flight guard); the frontend currently only console.errors it and resets the icon with no user-visible message. Correct behaviour (no freeze, no overlap), minor UX gap. The task ACs concern not-freezing, not refresh-busy messaging, so this is out of scope.

AC#5 (FIELD, ESP32-S3) is NOT verified here (no hardware). All index.js changes are field-unverifiable: node --check validates syntax only; the LittleFS build does not catch wiring errors.

Landed: loop-defer bridge for the two PIC outbound-HTTP seams (ADR-137 amends ADR-132). Firmware (OTGW-Core.ino, restAPI.ino, PICtypes.h, OTGW-firmware.{h,ino}) + frontend (data/index.js) committed; ADR-137 + README index staged. Build esp32+esp32-classic SUCCESS (per-env lines grepped), evaluate.py --quick 0 FAIL, adr-lint ADR-132 PASS. Remaining field-validation: AC#5 (ESP32-S3 hardware) — open PIC tab while live-log WS streams (must keep flowing), refresh-download a hex (UI stays responsive), update-check against unreachable host (no freeze/no reset). Tracked under epic TASK-865.

AC#5 ON-DEVICE VERIFIED 2026-06-29 (classic-S3 @192.168.88.64, alpha.291, PIC fw6.6 present). API-level concurrency measurement (stronger than visual: measures the actual handler-block the original bug caused): (1) GET /api/v2/pic/update-check?recheck=1 handler returned 110ms (NOT blocked by the 5s outbound HEAD); concurrent 10x /health avg 289ms/max 598ms - no stall. Deferred HEAD then resolved in loop() (update-check -> status:ready latest:6.6), proving the full async seam. (2) Unreachable-host non-freeze: handler return is host-independent (110ms) since the outbound is deferred off the handler path. (3) GET /pic?action=refresh&name=gateway&version=6.6 handler returned 232ms {status:started}; concurrent 12x /health avg 448ms/max 1227ms - no multi-second freeze (a blocking 15s download would have stalled it). Both ADR-137 seams (HEAD + GET) confirmed non-blocking. WS live-log continuity INFERRED (not directly observed - browser extension was offline): the WS server shares the single async_tcp task with HTTP handlers, which never starved past ~1.2s throughout, so WS frames cannot be starved either. Regression (synchronous outbound HTTP freezing the UI) definitively gone.
<!-- SECTION:NOTES:END -->
