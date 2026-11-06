---
id: "ADR-212"
title: "Load the Web UI assets natively behind a bounded server-side file queue"
status: "Accepted"
date: "2026-11-06"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-139"
  - "ADR-147"
  - "ADR-165"
  - "ADR-188"
  - "ADR-192"
topics:
  - "web-server"
  - "backpressure"
  - "webui"
  - "first-paint"
components:
  - "WebQueuedFileResponse (webServerCompat.h)"
  - "platformTcpTxHoldingOnPort (platform_esp32.h)"
  - "v2.html asset loader and window.otgwAssetLoad diagnostics"
symbols:
  - "webSendFile"
  - "platformTcpTxHoldingOnPort"
context_scope: "selective"
format: "canonical"
---

# ADR-212 Load the Web UI assets natively behind a bounded server-side file queue

## Status

Accepted, 2026-11-06.
## Status History

```yaml
status_history:
  - date: 2026-11-05
    status: Proposed
    changed_by: unknown
    reason: "Recovered from the record as it stood before this transition; this ADR predates the status_history convention. The actor was never recorded and is not guessed here. The Status line read: Proposed (TASK-1659)."
    changed_via: unrecorded
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-139
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-147
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-165
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-188
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-192
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer accepted in session 2026-11-06 after the second bench run; heap dip follow-up in TASK-1673
    changed_via: adr-kit lifecycle
```


Accepted ADR-147, ADR-165 and ADR-188 stay in force. This decision narrows how ADR-188 counts connections (see Decision 3).

Open caveat: in an A/B test on one ESP32-S3 Combo unit, two browser tabs loading at the same time lowered the largest free heap block to 7,668 bytes, against 13,812 bytes on beta.94. That is below the 8,192-byte guard used by `/api/v2/device/info`. The cause is two files streaming at the same time: each stream holds up to one TCP (Transmission Control Protocol) send buffer (5,744 bytes) plus a chunk buffer of about the same size, and native loading reaches two streams with a single tab. The queue adds about 1 KB per waiting request. A single gzipped page avoids the dip; the measurements are in PR #697.

Maintainer decision, 2026-11-06 (Robert, in the session after the second bench run below): accept despite the open dip. The dip is not caused by the queue alone: dev without this change already falls to about 3.7 KB minimum free heap under the same two-tab load (TASK-1673). TASK-1673 tracks the reduction (a 512-byte stdio buffer per open file, about 3.5 KB per stream, and a file gate that also looks at total free heap). This decision narrows how ADR-188 counts connections (Decision 3); ADR-188 stays Accepted for everything else.

## Context

The Web UI needs five static files on first load: `v2-bundle.css`, `v2.js` and three WOFF2 (Web Open Font Format 2) fonts. The server streams at most two files at a time (ADR-165) and answers 503 when both slots are busy. Browsers do not retry a 503 for a stylesheet, script or font on their own.

Beta.94 (TASK-1643) handles this in JavaScript: a render-blocking stylesheet link with an `onerror` retry, then `v2.js`, then the fonts one by one, each retried up to eight times. This works for one tab, but every retry costs a full request, and the fonts can only start after the script has loaded.

## Decision

1. **Native loading.** The page uses a normal `<link rel="stylesheet">`, a deferred `<script>` and `<link rel="preload">` for the three fonts (`crossorigin="anonymous"`, matching the `@font-face` rules kept in `v2-bundle.css`). JavaScript retries only after a real load error.
2. **Bounded file queue.** When both file slots are busy, the server keeps the request instead of answering 503. At most four requests wait, in arrival order, for at most 10 seconds. A waiting request opens no file and allocates no send buffer. Requests resume from the existing AsyncTCP callbacks; nothing runs on the main loop or in a new task.
3. **Connection accounting.** ADR-188 counts every established web connection as a possible sender. A queued request cannot send, so it is excluded from that count, but only after matching both the live connection object and its owner, and only while it holds no unsent or unacknowledged data. One sender slot is reserved before a waiting request resumes.
4. **Unchanged limits.** Two active file streams (one under heap pressure), the REST (Representational State Transfer) API gate and the connection cap stay as they are. Queue overflow, deadline expiry and genuine memory or send-buffer pressure still return 503 with `Retry-After: 1`.
5. **Diagnostics.** `window.otgwAssetLoad` records attempts, retries and failures per asset for each page load. `/api/v2/device/info` and telnet report `hd_webfile_queued`, `hd_webfile_admitted`, `hd_webfile_cancelled`, `hd_webfile_timeout`, `hd_webfile_max_wait_ms`, `hd_webfile_pending` and `hd_webfile_pending_hwm`.

No AsyncTCP, ESPAsyncWebServer or lwIP code or configuration changes. Assets stay external files on LittleFS (ADR-139, ADR-192).

## Alternatives Considered

- **Keep the beta.94 JavaScript loader.** Smallest firmware, but loads are slower and two tabs can exhaust a font's eight retries (see Verification).
- **Raise the file or connection limits.** Rejected: ADR-147, ADR-165 and ADR-188 document the memory and receive-path failures this caused.
- **Cap connections at the TCP level (listen backlog).** Possible without rebuilding lwIP, but needs an AsyncTCP change, counts WebSocket and REST connections against the same small budget, and delays rejected clients by TCP retransmission timers. It does not replace the two-stream file cap.
- **Embed CSS (Cascading Style Sheets), JavaScript and fonts in the HTML (HyperText Markup Language) page.** Removes the extra requests, but changes caching and update behavior and adds base64 overhead.

## Consequences

- Faster first loads and fewer 503s for normal browsing (see Verification).
- Each queued request keeps its connection and request objects (about 1 KB of fixed objects each, plus headers). This is the most likely source of the lower heap minimum and is the open caveat above.
- More state in the file-serving path: queue order, deadlines, cancellation and slot release must stay correct. The host test covers these.
- A client that keeps sending data with a zero receive window can hold a queued request past the 10-second deadline; the queue size, not the socket lifetime, is what is bounded.

## Verification

Test gate: `tests/test_web_file_scheduling.py` compiles the production file-serving code on the host and checks queue order, limits, cancellation, deadline wraparound, sender reservation and exactly-once slot release. `tests/webui/native-asset-load.test.mjs` covers the page loader and diagnostics.

### A/B against beta.94, 2026-11-05

Same ESP32-S3 Combo unit, same settings; only the asset-loading code differs. 5 cold and 5 warm loads per browser, plus 5 rounds of two Chrome tabs loading at the same time. Measured from browser network events through a TCP tunnel.

| Metric | beta.94 | This change |
|---|---:|---:|
| Single-tab loads complete | 30/30 | 30/30 |
| Single-tab loads without any retry | 29/30 | 30/30 |
| Cold load, median time to all five assets | 3.3–4.2 s | 2.2–2.7 s |
| Two-tab loads complete within 30 s | 5/10 | 10/10 |
| Two-tab asset 503 responses | 60 | 7 |
| Minimum free heap | 16,392 B | 12,060 B |
| Minimum largest free block | 13,812 B | 7,668 B |

Both builds kept at most two file streams, with no WebSocket or MQTT (Message Queuing Telemetry Transport) drops and no reboot. This is short-run evidence from one unit.

### Second bench run, 2026-11-06 (TASK-1659 review)

Rebased on dev beta.106, OT-Direct ESP32-S3 bench unit, OTA (over-the-air update), settings unchanged. Python two-tab load: 2 tabs x 7 assets x 25 rounds, fresh connections, fresh heap window per build.

| Metric | dev beta.106 | This change |
|---|---:|---:|
| Requests answered 200 | 350/350 | 350/350 |
| Tab load p50 / p95 / max | 3.14 / 4.76 / 8.18 s | 3.17 / 3.37 / 3.45 s |
| Minimum largest free block (separate run) | 12,788 B | 5,364 B |
| Minimum free heap (separate run) | 3,772 B | 804 B |

Queue burst (8 concurrent GETs of `/v2.js`): 6 served after queueing (max wait 2,257 ms), 2 got 503 with `Retry-After: 1` because the queue was full; none hung. Disconnect while queued: `hd_webfile_cancelled` 4, free heap back to 69.6 KB, no reboot. `tests/webui/native-asset-load.test.mjs`: 9/9 passed in three consecutive runs. The 10 s deadline 503 was not observed on the bench.

## Related Decisions

ADR-139 (ETag caching), ADR-147 and ADR-165 (file-serving memory and parallelism), ADR-188 (connection cap; accounting narrowed here), ADR-192 (shipped asset set, unchanged).

## References

- PR #697: https://github.com/rvdbreemen/OTGW-firmware/pull/697 (André Brait), with the beta.94 A/B measurements.
- TASK-1659 (review, both bench runs) and TASK-1673 (heap dip at two tabs, follow-up).
- `tests/test_web_file_scheduling.py`, `tests/webui/native-asset-load.test.mjs`.
