---
id: TASK-1741
title: >-
  Two open Web UI tabs get ~16 REST 503s per minute: the v2 pollers exceed the
  2-in-flight REST cap (ADR-165) and the 4-sender TX cap (ADR-188)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-08T18:26:00Z'
updated_date: '2026-11-08T23:36:00Z'
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
- [x] #1 Explain per source (REST cap, TX cap, heap clamp) how many 503s two tabs cause, with the run A numbers as baseline
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

2026-11-08 otgw-firmware-80 red/green on int-157 (HEAD e4d0280f4) vs origin/dev 07b38ee49 + the new test file: client side: rest-503-keeps-state fix 3/3, old 0/3 (SAT page keeps last data on 503; wizard health Unknown not a false warning; SIMULATION badge stays). AC2 also needs red/green on .61; AC1/AC3 are measurements.

2026-11-08 bench .61 beta.157 soak (4 REST polls / 30 s plus a headless UI): 1 x 503 on /api/v2/settings in 81 calls, at the moment the UI loaded. Related finding: v2.js:6431 fetchSettings() at startup has no retry, so one busy 503 leaves the SAT page's 'SAT settings' empty until the Settings page is opened (seen on .61). AC1-3 measurements not done in this session.

2026-11-09 otgw-firmware-80, AC1 analysis (read-only subagent, code + run A numbers; reviewed in session). Measured run A split per 503 source with code explanation below; derived poll inventory fits the measured load within ~5 %.

Run A measured about 16 browser 503s per minute with two tabs. The code explains the cause. Each tab runs its own 2-slot API queue (a JS closure at v2.js:23-30), while the firmware's counters are global (restInFlight at restAPI.ino:46, a pcb walk in platform_esp32.h:465-481). So two tabs can offer 4 concurrent requests against a REST cap of 2 and a TX cap of 4.

Run A split, measured:
- REST cap 2 on a healthy heap: 9.8/min.
- ADR-188 TX cap: 6.0/min.
- Heap clamp to cap 1: 0.16/min.
- Unattributed: 0.18/min.

That split comes from whichever gate fires first, not from independent causes. webConnCapExceeded() runs before the in-flight check (restAPI.ino:2822 vs 2827), so a request that breaks both caps is logged as WEB CONN CAP. Raising the TX cap alone would turn many of those 269 into REST BUSY 503s rather than remove them.

The poller inventory fits the measured load. The pollers I derived for Monitor > Values on an OT-Direct board add up to about 108-120 requests/min for two tabs; run A measured 114/min.

The 503s fall mostly on the slow pollers (about 23-58 % of their calls) and hardly on otmonitor (2 %). That fits a phase effect. Every setInterval is registered in the same init() tick (v2.js:6419-6502), so every 30 s each tab fires otmonitor, device/time, health, sat/status and devClock at once, plus weather every 60 s, plus the 3-request health chain. Each tab then fills its own 2 slots, and the other tab's requests hit a full server.

Two more effects add to this:
- **Release skew.** The client frees a slot when the body arrives (v2.js:62-63), but the server frees it only on disconnect (webServerCompat.h:127-131). So even a perfect cross-tab budget of 2 will not reach exactly 0.
- **Holders the queue doesn't count.** Every TX-cap 503 needs at least one connection outside the two queues: the logs show 5-7 HTTP holders (7-9 minus 2 ws), while the queues can have at most 4 requests open. That leaves at least 1-3 extra holders, and the queues admit at most 2 REST slots.

**Basis checked:** the run A firmware (beta.149, 3dc9dfa20) and the code read (int-157 plus the uncommitted TASK-1743 diff) have the same poller periods and cap constants. Between the two, git diff shows only the one-time i18n pack load, the fetchSim 503 handling and the TASK-1743 startup settings retry.

**Cheapest AC2 candidates:**
- Random start offset per tab on the init timers, plus staggering inside one tab. A few lines. It breaks the phase alignment.
- MAX_IN_FLIGHT 1 per tab. A one-line change, and it matches ADR-165's original premise (client N=1, ADR-165:49-53). The cost is a slower page start, and 3 tabs still overload.
- A document.hidden guard on the pollers that lack one. It does nothing for run A's two visible windows, but it does help a real background tab.
- A BroadcastChannel leader for otmonitor, which is about 49 % of all requests. This is the real N-tab fix but costs the most code. It only works within one origin.
- Pausing the queue on a 503 (Retry-After: 1) without retrying. This is near Robert's 2026-11-08 decision "R1 only, no retry in the API queue", so it needs his OK.

**AC3:** changing a cap, or adding a cross-tab contract, needs Robert's decision with measured numbers. ADR-165's data covers esp32-classic only. ADR-188 claimed hd_webconn_503 = 0 at nominal load, which run A contradicts. ADR-184 excludes the v2 bundle until a capture shows the burst there, and run A is that capture.

NUMBERS: MEASURED (run A, from the task notes, 45 min, 2 tabs):
- Total load: 5140 requests = 114.2/min. 503s: 725 = 16.1/min (14.1 %). otmonitor: 2506 = 55.7/min.
- By telnet line:
  - REST BUSY at cap 2 (maxblk 17396-31732): 441 = 9.8/min.
  - WEB CONN CAP: 269 = 6.0/min.
  - Cap-1 heap clamp: 7 = 0.16/min.
  - 725 - 717 = 8 browser 503s with no telnet line: 0.18/min.
  - The task-1736 notes say 448 REST BUSY lines; 441 + 7 = 448, which agrees.

DERIVED FROM CODE (estimate): offered load per tab on Monitor > Values, OT-Direct board.

| Endpoint | Per tab /min | Basis |
|---|---|---|
| otmonitor | 30 | 2 s |
| health | 4 | 15 s |
| simulate | 4 | 15 s |
| otdirect/overrides | 4 | 15 s |
| otdirect/status | 4 | 15 s |
| sat/ble/discovery | 2.4 | 25 s, + retry on failure |
| sensors | 2.4 | 25 s, + retry on failure |
| sat/status | 2 | 30 s |
| sat/weather | 1 | 60 s |
| device/time | 6 or ~0.1 | 10 s when the WS carries no frames for 15 s; ~0.1 when frames flow |

- Total per tab: ≈ 59.8/min (silent-WS branch) or ≈ 54/min (frames branch). Two tabs: 120 or 108/min, against 114 measured. The inventory fits within about ±5 %.
- device/time: 70 503s cannot come from the frames branch, which makes about 9 calls in 45 min for 2 tabs. So on .61 the silent-WS branch (psCheckTick) was active at least part of the time. This is an inference, not verified.

ESTIMATE: 503 fraction per endpoint = measured 503s ÷ derived calls (2 tabs, 45 min).

| Endpoint | 503s ÷ calls | Fraction | Note |
|---|---|---|---|
| sat/weather | 52/90 | 58 % | |
| sat/status | 91/180 | 51 % | |
| ble/discovery | 80/216 | ≤37 % | retries enlarge the denominator, so an upper bound |
| otdirect/overrides | 117/360 | 33 % | lower bound: a failed health request skips the chain |
| simulate | 85/360 | 24 % | lower bound, same reason |
| otdirect/status | 83/360 | 23 % | lower bound, same reason |
| sensors | 50/216 | ≤23 % | upper bound, same as discovery |
| device/time | 70/540 | 13 % | |
| health | 34/360 | 9 % | |
| otmonitor | 51/2506 | 2 % | measured denominator |

- The slow pollers fire together at the 30 s / 60 s boundaries of each tab's init() phase. Each tab then offers 4-5 requests at once, filling its own 2 slots, and the other tab's requests hit a full server. This pattern is consistent with phase alignment and inconsistent with random overlap.

ESTIMATE: 503s per minute per source.
- **One tab:**
  - REST cap: ≈ 0. The queue holds it to ≤2; only the release-skew race remains. Supporting data: ADR-184's classic capture had 0 503s with the queue, and the beta.157 soak had 1 × 503 in 81 calls at UI load.
  - TX cap: ≈ 0. Two queued requests plus a few closing pcbs stay ≤ 4.
  - Heap clamp: only during maxblk < 16000 dips.
- **Two tabs:**
  - REST cap: the measured 9.8/min is the baseline. Up to 2 of the 4 offered requests have no slot whenever both tabs burst together.
  - TX cap: 6.0/min measured. Mechanism (derived): the queues alone reach at most 4, which is not > 4. So every TX-cap 503 needs at least one connection outside the queues, such as closing pcbs, 503 replies still draining, or WS handshakes in progress. The logs show 5-7 HTTP holders, so 1-3 such extras. Which kind dominates is not derivable from code.
  - Heap clamp: 0.16/min. Small because the dips below 16000 are short; the run A per-window minimum was 11764-26612.
  - 8192 B gates: expected 0 in run A, because the measured minimum maxblk was 11764 > 8192.
- **ADR-172 429s (estimate):** two tabs offer about 60 otmonitor/min against a global 40/min budget. So up to about 1/3 of otmonitor answers would be 429. These are not 503s, but each one holds a REST slot while it is answered.

FACTS:
- The v2 API queue wraps window.fetch with MAX_IN_FLIGHT = 2. The inFlight/waiting state lives in a closure in each page, so nothing is shared across tabs. [D:/Users/Robert/Documents/GitHub/RvdB/wt-1689/src/OTGW-firmware/data/v2.js:23-30, 46-50, 72-78]
- The client queue frees a slot when the response body has arrived. A 20 s valve frees a stalled slot without aborting the request. [v2.js:52-63 (release via response.clone().arrayBuffer().then(release, release) at v2.js:63; STALL_RELEASE_MS at v2.js:28)]
- The server frees the REST slot only in the request's onDisconnect callback. The server sends Connection: close after every response. [src/OTGW-firmware/webServerCompat.h:127-131 (webArmDisconnect -> restSlotRelease), restAPI.ino:2813-2820 comment, restAPI.ino:2836-2837 (g_restSlotHeld, webArmSlotRelease)]
- The REST cap is REST_MAX_INFLIGHT 2, held in a global static counter restInFlight. A request that arrives while restInFlight >= the effective cap gets a 503 'REST BUSY' with Retry-After: 1. [restAPI.ino:43-46, 2826-2833, 179-185]
- The heap clamp lowers the cap to 1 when maxfreeblock < 16000, and to min(cap,2) when < 24000. With the shipped cap of 2, only the <16000 step changes anything. [restAPI.ino:55-64]
- The ADR-188 TX cap returns a 503 when the port-80 connections holding TX, minus the WebSocket clients, exceed WEB_MAX_TX_CONNECTIONS = 4. It is checked BEFORE the REST in-flight cap, so a request that breaks both caps is logged as WEB CONN CAP. [restAPI.ino:80-95 (webConnCapExceeded), restAPI.ino:2822-2833 (order of the gates)]
- A port-80 pcb counts toward the TX cap when it has unsent or unacked segments, or is ESTABLISHED and is not a queued idle file waiter. The request being admitted counts too. [src/libraries/Platform/src/platform_esp32.h:465-481; docs/adr/ADR-188-*.md:103-108]
- Other 503 paths: an 8192 B maxfreeblock gate on sat/status, graph/history, debug dump, device/info and diag download, plus 'history busy'. None of them writes a REST BUSY or WEB CONN CAP telnet line. [SATcontrol.ino:4236, graphHistoryStuff.ino:92 and 95, restAPI.ino:2221, 3155/3210, 3800]
- The ADR-172 rate check runs after restInFlight++, so a 429 answer also holds a REST slot. The otmonitor budget is global: 1 per 1500 ms, burst 2. [restAPI.ino:2834-2837, 2899-2905, 2661-2665, 2682-2684]
- Pollers registered in init() in the same tick: tick 1 s (no fetch), fetchConn 15 s, fetchSeed 20 s only when the WS is closed, otmonTick 2 s, psCheckTick 5 s, devClockTick 30 s, fetchSatStatus 30 s, fetchWeather 60 s. startDiscoveryPolling runs BLE discovery at 10 s + 25 s*k and sensors at 14 s + 25 s*k. [v2.js:6419, 6436, 6438, 6494, 6497-6502, 5830-5838]
- fetchConn requests /v2/health. On success it fires fetchSim (/v2/simulate) and fetchOverrides (/v2/otdirect/overrides on OT-Direct) together, then otdirect/status chained after the overrides. A failed health request (503) skips all three follow-ups. [v2.js:1846-1848, 1915-1919, 541-549, 2142-2151, 1959-1965]
- otmonTick fetches otmonitor only when the tab is visible and Monitor > Values is open (or PS=1), with at most one request in flight per tab. A 429 pauses that tab for retry_after. [v2.js:3473, 3480-3494, 3518-3523]
- psCheckTick asks for device/time every 10 s, but only after 15 s of WS frame silence (or while PS=1). devClockTick asks once per 600 s (re-arms on failure). Both skip hidden tabs. [v2.js:3532-3552, 341, 360-368, 675]
- These pollers have NO document.hidden guard: fetchConn (4 requests/15 s), discovery, sensors, fetchSatStatus and fetchWeather. Only otmonTick, psCheckTick and devClockTick check document.hidden. [v2.js:1846, 5779, 5808, 3744, 3766 vs 3519, 3535, 361]
- The SAT page polls sat/status every 5 s through fetchWithRetry (up to 3 tries). Settings > Sensors polls the BLE roster every 4 s. [v2.js:5001, 5018-5019, 98-111, 3206-3212]
- Run A measured on bench .61 (OTGW32, no PSRAM, beta.149), 45 min, two visible headless windows on Monitor > Values: 5140 REST requests, 725 browser 503s, 2506 otmonitor GETs. Per endpoint: overrides 117, sat/status 91, simulate 85, otdirect/status 83, ble/discovery 80, device/time 70, weather 52, otmonitor 51, sensors 50, health 34, other 12. Telnet: 441 REST BUSY cap 2, 269 WEB CONN CAP with 7-9 holders (2 ws), 7 cap 1. maxblk minimum 11764. [backlog/tasks/task-1741*.md Description; backlog/tasks/task-1736*.md:74-79]
- Between beta.149 (3dc9dfa20) and HEAD, the v2.js diff changes no poller period, MAX_IN_FLIGHT or document.hidden guard: only the one-time i18n pack retry and the fetchSim 503 handling. restAPI.ino and webServerCompat.h show no change to the cap constants. The uncommitted TASK-1743 diff only adds a retry to the one-time startup settings fetch. [git diff 3dc9dfa20 HEAD -- src/OTGW-firmware/data/v2.js restAPI.ino webServerCompat.h (grep on poller and cap lines); git diff -- v2.js at v2.js:2629-2633]
- ADR-165 premise: the client kept N=1 single flight; a client MAX_INFLIGHT knob was left unbuilt. Its data covers esp32-classic only, and generalizing to esp32-otgw32 is unverified. [docs/adr/ADR-165-optimal-request-parallelism-esp32s3-webui-rest.md:49-57]
- ADR-184 excludes the v2 bundle until a capture shows the same burst there. Yet the v2.js header cites 'ADR-184 for the v2 bundle (TASK-1210)', and the ADR has no amendment for v2. [docs/adr/ADR-184-*.md:105, 135, 44-65; v2.js:15]
- ADR-188 recorded hd_webconn_503 = 0 at nominal load (three clients polling device/info and settings). Run A's two-tab v2 load produced 269 TX-cap 503s. [docs/adr/ADR-188-*.md:141-144; task-1741 Description]
- MDN marks the Web Locks API as available only in secure contexts (HTTPS), so it is unusable on http://<ip>. MDN describes BroadcastChannel as same-origin and shows no secure-context banner for it. [https://developer.mozilla.org/en-US/docs/Web/API/Web_Locks_API ; https://developer.mozilla.org/en-US/docs/Web/API/BroadcastChannel (fetched 2026-11-09)]

NEXT:
- AC2, cheapest option and a first A/B arm: give each tab a random start offset (0 to the period) on the init setIntervals, and stagger the tab's own 30/60 s cluster so it never offers more than 2 at once (v2.js:6436-6502). It costs a few lines, adds no requests and stays inside ADR-184's 'no prioritise/batch' rule. It removes the phase-locked bursts that drive the 23-58 % rejection rate of the slow pollers. Expected effect: fewer REST-cap and TX-cap 503s. It cannot reach 0, because random overlap and the release skew remain.
- AC2 arm B: MAX_IN_FLIGHT 1 per tab (v2.js:27), a one-line change. Two tabs then offer at most 2 = the REST cap, which also keeps the TX count low. This matches ADR-165's original client premise of N=1 (ADR-165:49-53); it does not contradict it, which corrects the earlier note in TASK-1741. Costs: single-tab startup and panel bursts take twice as many rounds, and 3 tabs overload again. Measure it against arm A with the same ab_bench harness on .61.
- AC2 add-on that the run A harness cannot show: add a document.hidden early return to fetchConn, startDiscoveryPolling's pollers, fetchSatStatus and fetchWeather, and refresh once on visibilitychange. A real background tab then drops from about 24/min to 0. Both run A windows were visible, so the red/green harness would show no gain. A separate scenario (one visible, one hidden tab) is needed to evidence it.
- AC2, larger option: a BroadcastChannel leader tab that polls otmonitor, and later the fetchConn chain, and broadcasts the result. otmonitor is about 49 % of all requests (2506 of 5140 in run A) and is fetched twice for identical data. Limits: same-origin only, so an IP tab, a hostname tab and an HA ingress tab do not coordinate. It needs leader failover. It changes the client contract and likely needs its own ADR. Web Locks is ruled out on plain http (MDN: secure contexts only).
- Pausing queue dispatch on a 503 for Retry-After (1 s) plus jitter, with no retry, would slow the cascade in which a rejected tab immediately fires its next queued request, adding TX holders. It sits next to Robert's 2026-11-08 decision 'R1 only, no retry in the API queue', so ask Robert before building it, framed as rate reduction rather than retry.
- Before AC2 red/green, check the run A CSV/summary.json (logging-session scratchpad t1736, outside this worktree) for: the 429 count, whether the 5140 total includes 429s, the time offset between the two windows' page loads (phase lock), and the heap.csv sampling interval.
- Add a single-tab control run to the same harness (same page, 45 min) to measure the single-tab baseline that the release skew leaves. That sets a realistic AC2 target instead of exactly 0.
- AC3: any change to REST_MAX_INFLIGHT, WEB_MAX_TX_CONNECTIONS or the 16000 clamp goes to Robert as one question with measured run numbers. Note the first-gate-wins attribution: raising the TX cap moves 503s to REST BUSY rather than removing them. ADR-165's ceiling was measured on esp32-classic only, and ADR-188's nominal-load claim (0 webconn 503s) does not hold for two v2 tabs. Also put the ADR-184 scope gap to Robert: v2 is excluded until a capture shows the burst, and run A is that capture.

NOT VERIFIED:
- The 429 count in run A and whether the 5140 total includes 429s: the task notes report neither. The raw CSV is in another session's scratchpad, which I did not access.
- Whether ab_bench opened both windows at about the same moment, which would phase-lock the two tabs' timers. If it did, run A is close to a worst case.
- Whether .61's WebSocket carried OT frames during run A. The device/time branch is inferred from the 70 device/time 503s, not observed.
- Which connections make up the 1-3 extra TX holders behind the WEB CONN CAP 503s: closing pcbs with an unacked FIN or data, 503 replies still draining, WS upgrade handshakes in progress, or browser preconnects. Whether lwIP keeps the FIN segment in unsent/unacked until it is ACKed (so a closing pcb counts) is an lwIP detail I did not open.
- How large the client/server release skew is, and how often one tab collides with itself. There is no single-tab run A control; the only hints are ADR-184's classic capture and the beta.157 soak (1 × 503 in 81 calls).
- The cause of the 8 browser 503s with no telnet line (725 - 717). Candidates are telnet line loss or a short dip below 8192 between heap samples (sampling interval unknown). Unassigned.
- Whether BroadcastChannel needs a secure context: MDN shows no banner, but I did not check the spec or the browser-compat data. Also from memory only, not checked: browser background-timer throttling (Chrome intensive throttling of hidden tabs).
- All per-minute model numbers are estimates from code periods. No device, browser or test ran in this session (read-only constraints).
<!-- SECTION:NOTES:END -->
