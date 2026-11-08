---
id: TASK-1741
title: >-
  Two open Web UI tabs get ~16 REST 503s per minute: the v2 pollers exceed the
  2-in-flight REST cap (ADR-165) and the 4-sender TX cap (ADR-188)
status: To Do
assignee: []
created_date: '2026-11-08T18:26:00Z'
updated_date: '2026-11-08T18:51:00Z'
labels:
  - bug
  - webui
  - rest
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 847000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Measured in TASK-1736 run A, 2026-11-08 18:36:57-19:21:57, on bench .61: OTGW32 without PSRAM, beta.149. Two headless browser windows had Monitor > Values open; otmonitor polled every 2 s per tab. Over 45 min the browser got 725 HTTP 503s out of 5140 REST requests, about 16 per minute or 14 %. They hit every polled endpoint: otdirect/overrides 117, sat/status 91, simulate 85, otdirect/status 83, sat/ble/discovery 80, device/time 70, sat/weather 52, otmonitor 51, sensors 50, health 34, others 12.

Cause, from the 717 matching telnet lines:
- 441 (62 %): 'REST BUSY: 2/2 in-flight (cap 2)' at maxblock 17396-31732. That is the normal REST_MAX_INFLIGHT=2 ceiling (restAPI.ino:44, ADR-165) on a healthy heap.
- 269 (37 %): 'WEB CONN CAP: 7-9 connections hold TX (2 ws, cap 4)'. That is ADR-188's WEB_MAX_TX_CONNECTIONS=4 (restAPI.ino:82-95).
- 7 (1 %): cap 1 from the heap-tier clamp below maxblock 16000 (restEffectiveInflightCap, restAPI.ino:55-64, ADR-149/TASK-884).
So the 503s are NOT mainly the low-heap clamp. Two tabs offer more parallel requests than the server's fixed caps admit.

ADR-165 (Accepted, N*=2) assumed the v2 client already stays within 2 in flight. It names a client-side MAX_INFLIGHT knob as unbuilt future work, and it was measured on esp32-classic only. With two tabs, each running independent setInterval pollers (v2.js:6305-6320 and the page pollers), that assumption fails.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Explain per source (REST cap, TX cap, heap clamp) how many 503s two tabs cause, with the run A numbers as baseline
- [ ] #2 A fix (client-side request scheduling in v2.js and/or a reviewed cap change) brings two-tab 503s close to 0 without heap regression; red/green evidence on .61
- [ ] #3 If a cap or ADR changes, Robert decides with measured numbers (ADR-165/188 are Accepted)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Related: TASK-1736 (heap fragmentation, same bench), TASK-1740 (2.0.0 stable readiness gate). Run A files: runs/run-A-2026-11-08-19-22-00.csv/.summary.json, logging session scratchpad t1736 (ab_bench.mjs).

2026-11-08 analysis, code only (dev 9805336fa):
- Root cause: v2.js runs its own per-page API queue with MAX_IN_FLIGHT = 2 (v2.js:15-83, ADR-184), exactly the server's REST_MAX_INFLIGHT = 2 (restAPI.ino:44, ADR-165). Each tab owns 2 slots and nothing coordinates across tabs, so two tabs offer up to 4 concurrent /api/ requests against a server cap of 2. That is the 441 cap-2 rejections on a healthy heap. The queue frees a slot when the body has arrived on the client, while the server still counts the connection as holding TX until lwIP has drained and ACKed (2 x MSS per connection, webServerCompat.h:165, TASK-1673). So 2 tabs x 2 slots plus draining connections reach the 7-9 TX holders of the 269 WEB CONN CAP rejections (WEB_MAX_TX_CONNECTIONS = 4, restAPI.ino:82-95, ADR-188).
- Where the thresholds come from: cap 2 is ADR-165 (Accepted; TASK-1015 load test, esp32-classic only; it assumed the client stays within 2). The 16000/24000 heap-tier clamp is ADR-149 / TASK-884 (restAPI.ino:48-64), not ADR-188 or ADR-167. The TX cap of 4 is ADR-188. The file gate (WEB_FILE_MAX_INFLIGHT 2, clamp at maxblk < 16000 or free < 24000) is restAPI.ino:76-110 (ADR-147, TASK-1673).
- Lowering the 16000 threshold: covers only 7 of 717 rejections (1 %), so almost no gain. Risk: the clamp exists to stop two large responses competing for contiguous blocks during dips (TASK-884 flood reboot, ADR-145). Not worth it.
- Raising the REST cap to 3-4: contradicts ADR-165's measured ceiling. On no-PSRAM .61 each chunked response holds a 2.3-2.7 KB snapshot plus a ~2.9 KB send buffer (TASK-1736 sizes), so 4 in flight is about 20 KB, during dips that already reach maxblk 11764. Real risk; would need a new load test on otgw32.
- Raising the TX cap: each sender can hold 2 x MSS = 2872 B in lwIP. TASK-1673 found the two-tab dip at 3.7 KB free heap. Risky.
- Presizing (TASK-1736 B) alone: does not touch the 98 % of 503s that occur at maxblk >= 17396. It may shorten each request's time in flight a little (no resize staircase), so collisions may drop somewhat. That is plausible, not measured; run B will show it. It can reduce the 7 cap-1 cases by making the dips shallower.
- Fix direction (my read, for discussion): the client offers more than the server admits, so fix it on the client. Options: (1) MAX_IN_FLIGHT 1 per tab: two tabs then fit cap 2, three do not; a single tab gets slower start-up. (2) Cross-tab coordination via BroadcastChannel (works on plain http). Web Locks (navigator.locks) would be simplest, but from memory it needs a secure context, so it is unusable on http://<ip>; to verify. (3) Lower the poll rate when the page sees 503s (back-pressure). Current pollers mostly use plain fetch; fetchWithRetry (v2.js:100-111, TASK-1033) retries 503 sequentially but adds requests. Any server cap change goes to Robert with run B's numbers.

2026-11-08: is a 503 visible to the user? Code reading of v2.js (dev 9805336fa), the 10 endpoints with the most 503s in run A.
SILENT (most): the handler maps !r.ok to null and skips this poll, so the previous values stay on screen and the next poll repairs them. Applies to otdirect/overrides (1887), otdirect/status (1856), sat/ble/discovery (2978, 3256, 5610), device/time (264, 3382), sat/weather (3606, 4890), sensors (3244, 5639), health (1743), and home sat/status (3586). Effect: data up to one poll interval older. In run A sat/status had 91 503s against about 180 polls of the 30 s home poll in 2 tabs, so roughly half of those polls were dropped silently: the home dial's sat.owns and target could lag 30-60 s. That is an estimate; the counts mix callers.
otmonitor (3326): silent, values keep showing; otmonLastOkMs is not advanced. 51 of 2506 polls (2 %), negligible.
VISIBLE, and two are misleading:
V1. SAT page (fetchSatPage, 4855-4877): fetchWithRetry tries 3 times (0 / +400 / +800 ms). If all three get 503, the page shows 'Couldn't load SAT status — retrying…' and HIDES the whole SAT body (4870), even when satLastData already holds good data (only the catch path at 4881 checks satLastData). One busy burst blanks the SAT page until the next 5 s poll.
V2. SAT setup wizard health check (loadHealth, 5921-5939): a 503 on health or sat/status turns into h = {} / s = {}, so the wizard shows OT 'not connected' and MQTT 'down' in warning colour (5932-5934, rows at 5941-5946) on a healthy system. A false alarm during onboarding.
V3. SIMULATION header badge (439-443): a 503 sets active = false and HIDES the badge while a simulation runs. Misleading, but only when simulation is on.
Run A cannot show any of these: Monitor > Values was open, not the SAT page or the wizard, and the bench captured no DOM state or screenshots. NOT VERIFIED on a device.

RECOMMENDATION (KISS), for Robert:
R1 (do): make a 503 'no change' in the three visible handlers:
  - V1: keep the body and the last data when satLastData exists; show the no-data banner only at first paint or on 404.
  - V2: keep 'unknown' (grey, not warning) for a field whose request failed, instead of false.
  - V3: leave the badge as it is on !r.ok.
  Three small edits, zero load change, and it removes everything user-visible found here.
R2 (optional, measure first): in the shared API queue (v2.js:23-83), re-queue a 503 response once after Retry-After (the server already sends 'Retry-After: 1', restAPI.ino:179-183) plus 0-500 ms jitter, and only when nothing else is waiting. Keep MAX_IN_FLIGHT 2. That turns most dropped polls into late ones. Cost: up to one extra request per 503 (run A: +725 over 45 min, about +14 %), spread by the jitter so two tabs don't retry in lockstep. A retry storm is bounded at one retry per request.
NOT recommended now:
  - MAX_IN_FLIGHT 1: halves single-tab parallelism, against ADR-165's measured optimum; slower page start.
  - Cross-tab BroadcastChannel coordination: real fix for N tabs, but too much code for this.
  - Server cap changes: see the analysis above.
The coordinator's guess (back-off with jitter, MAX_IN_FLIGHT 2) is R2. R1 is what fixes what the user actually sees; R2 only makes the data fresher.

2026-11-08 decision (Robert, relayed by the coordinator): R1 only, no retry in the API queue.
Implemented in lane-1741 (wt-1741), v2.js only:
- fetchSim: a 503 leaves the SIMULATION badge as it is (GET /v2/simulate exists on every board since TASK-1071/1294, so a 503 only means busy). The stale 'PIC-path only' comment is corrected.
- fetchSatPage: a non-404 failure after good data (satLastData set) keeps the body and data; the banner stays for first paint and 404.
- Wizard loadHealth/healthRow: a failed /health or /sat/status request gives grey 'Unknown' rows (ok === null) instead of 'Offline'/'Not connected'/'Not detected', and no outside-temperature warning.
- page-harness: { __status } support, byte-identical to lane-1732's change, so the merge is clean.
Tests: tests/webui/rest-503-keeps-state.test.mjs, three cases. Red on dev data (OTGW_DATA_DIR = main checkout): 3/3 fail with the exact defect ('Couldn't load' banner over data; 'OpenTherm bus Offline'; badge hidden). Green on the fix: 3/3. python evaluate.py --quick: 85 passed, 0 failed. Not verified on a device (the 1741 checks on .61 come after the single build).
<!-- SECTION:NOTES:END -->
