---
id: "ADR-300"
title: "Admit REST requests by measured memory need, up to 4 in flight (supersedes ADR-165, amends ADR-184)"
status: "Proposed"
date: "2026-11-26"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-165"
  - "ADR-184"
  - "ADR-188"
topics:
  - "rest-api"
  - "concurrency"
  - "admission control"
  - "heap"
  - "backpressure"
  - "webui"
aliases:
  - "rest cap 4"
  - "requirement rule"
  - "restrule 1"
  - "memory rule"
  - "TASK-3104"
components:
  - "REST admission gate"
  - "rest_max_inflight"
  - "v2 API queue"
symbols:
  - "restEffectiveInflightCap"
  - "REST_MAX_INFLIGHT"
  - "REST_RULE_BNEED"
  - "REST_RULE_FNEED_BASE"
  - "REST_RMAX"
  - "MAX_IN_FLIGHT"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-300 Admit REST requests by measured memory need, up to 4 in flight (supersedes ADR-165, amends ADR-184)

## Status

Proposed, 2026-11-26.

Guideline-level per ADR-080: the host test named under Verification compiles the real admission function and is the
gate; it is not wired into evaluate.py. On acceptance this ADR replaces ADR-165 as a whole: its REST part is changed
here, its static-file cap of 2 is carried over unchanged (Decision, last item).

## Status History

```yaml
status_history:
  - date: 2026-11-26
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

Terms used below: REST (the device's HTTP API, application programming interface), HTTP (Hypertext Transfer
Protocol), UI (user interface), BLE (Bluetooth Low Energy), PSRAM (external pseudo-static random-access memory),
PIC (the gateway's Microchip microcontroller), OT (OpenTherm), TX (transmit), TCP (Transmission Control Protocol),
IDF (Espressif IoT Development Framework), TASK (a backlog task id). Evidence labels: *measured* = from a bench run or
a build, with its file; *read* = read in the source, with `file:line` on `origin/dev` 42ac7203a unless marked;
*estimate* = reasoning, not measured.

The device admits a REST request only while fewer than a cap are in flight. ADR-165 (2026-07-06) set that cap to 2,
from a load test on whole-buffer responses. The admission function then lowers it further by the largest free block
of internal heap (*read*, `src/OTGW-firmware/restAPI.ino:83-93`): below 16000 B it admits 1, below 24000 B at most 2.

- The premise of those two thresholds is written next to them (*read*, `restAPI.ino:76-82`): "The biggest REST
  response (~8.6 KB settings) needs a contiguous block to buffer". Since TASK-883 the settings response is streamed
  in chunks and no longer buffered whole (*read*, `restAPI.ino:4173-4178`, `restSendChunked`). The premise no longer
  holds.
- *measured* (TASK-3104, AC2): one request costs about 5.5 to 9 KB of internal heap, nearly independent of the body
  size. A cbuf of 8192 B took 284 B internal and 8452 B PSRAM (`POST /api/v2/debug/cbufprobe`, .63), because large
  allocations go to PSRAM (`CONFIG_SPIRAM_USE_MALLOC=1`, always-internal limit 4096).
- *measured* (TASK-3104 series 4, .63): the largest free block after boot depends on the boot. Boot A had 26612 B,
  boot B 15860 B (BLE on, same firmware). On boot B the shipped rule ran at 1 in flight in 165 of 165 one-second
  samples, although about 18 KB of free heap was left.
- The board class that matters is BLE on or off, not PSRAM: BLE costs about 46 KB of free heap (*measured*, A/B on
  .63, TASK-3104 section 7).
- Independent of the REST cap, ADR-188 refuses a request while more than 4 connections on port 80 hold send data
  (`WEB_MAX_TX_CONNECTIONS`, *read*, `restAPI.ino:111-130`). That check runs before the REST cap (*read*,
  `restAPI.ino:2814-2817`).

Robert asked to scale the REST API (2026-11-26, interactive: "die taak voor de rest api opschalen heeft prioriteit")
and chose to do it before the stable if the sweep passes (question 14, answer "C", DM 1575515152768307362).

## Decision

**Decision maker:** Robert van den Breemen (maintainer). Question 16 (DM, Discord direct message, 1575589880514347149, 20:11 local,
2026-11-26) offered A "Geheugenregel, max 4 tegelijk. Nieuwe ADR (ik schrijf hem, jij accepteert), web UI leest het
getal van de unit (standaard 2), de kernregel "max 2 API-calls" verandert in alle drie de instructiebestanden, 60
minuten soak en eigen bèta vóór de stable. Je neemt de twee gemiste eisen bewust op je (geen vrijstelling van mij).",
B "Alles blijft op 2" and C "Alleen de regel verbeteren zonder het plafond te verhogen". Robert answered verbatim "A"
in DM 1575597641675575408 (20:42 local) and again "A" in DM 1575601390645157970 (20:57 local). Both messages have no
reply reference; they are the next messages after question 16 (checked through the Discord API by otgw-architect).
That answer chooses the route; this ADR still needs his acceptance.

The device admits a REST request by measured memory need, up to 4 in flight:

1. With nothing in flight, one request is always admitted (liveness).
2. With one or more in flight, a new request is admitted while all three hold:
   - fewer than `REST_MAX_INFLIGHT` = 4 are in flight;
   - the largest free internal block is at least `REST_RULE_BNEED` = 8192 B;
   - the free internal heap is at least `REST_RULE_FNEED_BASE` + `REST_RMAX` = 8192 + 14336 = 22528 B.
3. The 16000 B and 24000 B thresholds go.
4. The ADR-188 send cap stays at 4 connections. In series 4 it, not memory, set the real concurrency (in-flight high
   water mark 4 in every arm with the new rule).
5. The device reports its cap as `rest_max_inflight` in `GET /api/v2/health`. The v2 UI reads it once at start and
   uses it as its queue size; when the field is missing or not a number from 1 to 8 it uses 2, so a newer UI on an
   older firmware stays at today's limit.
6. The static-file cap `WEB_FILE_MAX_INFLIGHT` stays 2 (ADR-147, ADR-165's data); this ADR does not change it.

```c
// the rule, in the shape the implementation spec asks for (TASK-3104)
static inline uint8_t restEffectiveInflightCap() {
  if (REST_MAX_INFLIGHT <= 1 || REST_MAX_INFLIGHT >= 255) return REST_MAX_INFLIGHT;
  if (restForceStarved()) return 1;
  if (restInFlight == 0) return 1;                              // 1. always one
  if (platformMaxFreeBlock() >= REST_RULE_BNEED &&
      platformFreeHeap() >= REST_RULE_FNEED_BASE + REST_RMAX)   // 2. room for one more
    return REST_MAX_INFLIGHT;                                   //    up to 4
  return restInFlight;                                          //    nothing new
}
```

**Measured basis** (.63: Classic with an ESP32-S3 module, PIC, PSRAM; firmware beta.453+564ac47 with prerelease
switches; files in `OTGW-field-data/bench-63/2026-11-26_rest-study/`, analysis in
`OTGW-field-data/_comms/task3104_ac2_ac4.md` sections 9 and 12 to 15):

| Run | Shipped rule | New rule (tx cap 4) | Gain |
|---|---|---|---|
| Boot A, BLE on, block 26612 | cap 2 | min free 14.7 / 14.3 KB | +14 to +38 % successes |
| Boot B, BLE on, block 15860, offered 4 | cap 1, 1970 ok, worst p99 3.66 s | 4809 ok, p99 0.59 s, min free 14.1 KB | +144 %, 503s -70 % |
| Boot B, BLE on, block 15860, offered 8 | cap 1, 1393 ok, p99 3.1 s | 2615 ok, p99 1.6 s, min free 15.6 KB | +88 %, 503s -21 % |
| BLE off, block 34804 | no baseline in this boot | offered 8: about 5649 ok, min free 59.8 KB | not claimed |

In every series 4 arm: 0 reboots, `cmd_dropped` 0, WebSocket and MQTT (Message Queuing Telemetry Transport) drops 0, longest loop gap 211 ms (limit 500 ms).

**Two TASK-3091 criteria failed with BLE on, accepted by Robert, 2026-11-26 DM 1575601390645157970, against a cap-1
baseline:**

- heap at most 25 % worse than the baseline: -36 % at offered 4 (14.1 against 21.9 KB free). The baseline ran at 1 in
  flight, the new rule at 4. In absolute terms 14.1 KB equals the lowest point of the shipped rule in boot A (14.0 KB).
- at least 30 % fewer 503s: -21 % at offered 8. Of the 5729 refusals 5323 came from the ADR-188 send cap.

Question 16 named both failures and said they are not waived by the agent; Robert's "A" takes them on.

## Decision Contract

### Must

- Implement the rule of the Decision in `restEffectiveInflightCap()` (`restAPI.ino`) in every build, with
  `REST_MAX_INFLIGHT` 4, `REST_RULE_BNEED` 8192, `REST_RULE_FNEED_BASE` 8192 and `REST_RMAX` 14336, each constant
  citing TASK-3104 and this ADR.
- Read the heap only through the platform shims (`platformMaxFreeBlock()`, `platformFreeHeap()`), never through a raw
  `heap_caps_*` call in `restAPI.ino`.
- Keep the check order: ADR-188 send cap first, then this rule (`restAPI.ino:2814-2817`).
- Keep the `-DREST_MAX_INFLIGHT=255` raw-arm sentinel and the TASK-2921 starved-heap bench key.
- Report `rest_max_inflight` (the compile-time `REST_MAX_INFLIGHT`) in `GET /api/v2/health`, documented in
  `docs/api` and the OpenAPI file in the same change.
- In `v2.js`, take the queue size from `rest_max_inflight`, read once at start; use 2 when the field is missing, not a
  number, or outside 1 to 8.
- Keep the 503 retry with backoff in the UI: the rule still refuses when memory is short.
- Change the core rule "The web UI keeps at most 2 API calls in flight" in CLAUDE.md, AGENTS.md and
  `.github/copilot-instructions.md` at once, after Robert accepts this ADR, to "The web UI keeps at most the number
  of API calls in flight that the device reports (`rest_max_inflight`), 2 when it reports none."
- Update the comment of the otmonitor/telegraf rate limiter (`restAPI.ino:2668`), which ties its burst of 2 to
  "REST_MAX_INFLIGHT 2 (ADR-165)"; the burst itself stays 2.

### Must Not

- Bring back a fixed maxblock threshold on the settings-buffer premise.
- Raise `WEB_MAX_TX_CONNECTIONS` above 4 under this ADR. With BLE on, tx cap 6 lowered the free heap to 9.5 KB
  (*measured*, series 3); that needs its own decision.
- Change `WEB_FILE_MAX_INFLIGHT`.
- Let the UI send more requests at once than the device reports.

### Exceptions

- Prerelease builds may keep the TASK-3104 switches (`restrule`, `restrmax`, `txcap`, `restlog`) to A/B against the
  old rule; a stable build compiles without them.

### Verification

- Host test `tests/test_rest_admission_rule.py` (to be written with the firmware change): it slices the real
  `restEffectiveInflightCap()` out of `restAPI.ino` with stub shims and runs a grid over in-flight count (0 to 4),
  largest block and free heap. It must:
  - go red on the code before this ADR for "maxblock 15860, freeheap 35000, in flight 1" (old rule: no admission;
    new rule: admit) and for "maxblock 19444, freeheap 35004, in flight 2" (old rule caps at 2);
  - pin liveness (in flight 0 always gives at least 1), the block floor (8191 refuses), the free floor (22527
    refuses) and the cap (in flight 4 refuses).
- A web UI test in one file: the queue uses `rest_max_inflight` when present and 2 when missing or invalid.
- Bench (otgw-bench-tester): 60 min soak on .63 with the final binary, the v2 UI open in 2 browsers; 0 reboots, 0
  crash logs, 0 WebSocket and MQTT disconnects, `hd_min_free_heap` at least 8192, 503 counters and p99 reported.

## Alternatives Considered

- **B. Keep the cap at 2 (ADR-165 unchanged).** No risk and no work, but on a boot with a fragmented heap the shipped
  rule falls to 1 while 18 KB is free (boot B), and every client queues behind one request (otmonitor p99 3.66 s).
  Robert did not choose it.
- **C. Fix the rule but keep the ceiling at 2.** Removes the false clamp to 1, so a tight boot gets 2 again. It leaves
  the measured room for 4 unused, and with BLE off the device had at least 56 KB free at 4 in flight. Robert did not
  choose it.
- **Up to 6 or 8 with a higher send cap.** BLE off passed at 6 in series 2 (8 failed on heap, -29 %). With BLE on, tx
  cap 6 lowered the free heap to 9.5 KB. One cap per board class would add a second code path for a gain only BLE-off
  units see. Rejected for now.
- **Keep the fixed thresholds and move them (for example 12000/16000).** Still a guess on the block, not on the
  measured need; boot B shows the block alone does not say whether there is room.

## Consequences

**Positive:**

- A unit on a tight boot is no longer clamped to 1: +88 to +144 % successes and p99 from 3.66 s to 0.59 s on boot B
  (*measured*). On a roomy boot the gain is smaller (+14 to +38 %, boot A).
- The admission rule rests on measured needs, not on a premise that TASK-883 removed.
- The UI limit follows the device instead of a second constant, so a later change of the cap needs no UI change.

**Negative:**

- More requests in flight lower the free heap: with BLE on the lowest point was 14.1 KB against 21.9 KB for the
  clamped baseline (accepted, see Decision). *Mitigation:* the free-heap test of the rule refuses below 22528 B with
  one or more in flight, and the heap emergency ladder of ADR-167 stays.
- A single v2 tab can now hold all 4 slots, so another client (Home Assistant, otmonitor, a second tab) meets more
  503s while that tab loads. *Mitigation:* every client retries a 503; the rate limiter for otmonitor/telegraf stays.
  *estimate*, not measured with a real browser; the 60 min soak with 2 browsers covers it.
- OT-Direct (OTGW32) is not measured. Its OpenTherm answer window is about 400 ms and a frame is dropped at 750 ms
  (*read*, `OTDirect.ino:960-967`). The longest loop gap on .63 was 211 ms. *Mitigation:* the soak and our own beta
  run before the stable; a loop gap above 500 ms on any unit stops the change.
- A unit without PSRAM is not measured. The rule reads the free heap, so it admits less there (*estimate*).
- One test unit; within one boot the runs drift by about 20 %.

**Release order** (plan of record, coordinator 2026-11-26 21:1x, one cause per release): beta N = the ADR-298 bridge
(guards and image marker on the old partition table), beta N+1 = the ADR-298 table change with the USB (Universal Serial Bus) round, beta
N+2 = this ADR, over the air only, after its host test, the 60 min soak and Robert's acceptance. All before the stable.

## Open Questions

- [ ] 1. Robert: acceptance of this ADR, including the core-rule change in CLAUDE.md, AGENTS.md and
  copilot-instructions.md.

## Related Decisions

- **ADR-165 (REST and static-file cap 2):** replaced by this ADR on acceptance. Its REST cap of 2 becomes up to 4 by
  memory need; its static-file cap of 2 is carried over unchanged.
- **ADR-184 (classic UI queue at 2 in flight):** amended. The v2 bundle uses the same queue (`v2.js:14-27`, TASK-1210);
  its size now comes from the device, default 2.
- **ADR-188 (send cap of 4 connections):** unchanged; it runs before this rule and sets the real concurrency.
- **ADR-240 (what the send cap counts):** unchanged.
- **ADR-147 (static-file gate):** unchanged.
- **ADR-167 (heap emergency ladder):** unchanged; it stays the last line under the rule.
- **ADR-298 (4MB partition change):** no technical link; shares the release order above.

## References

- TASK-3104 (REST concurrency feasibility) and its notes; TASK-3091 decision rule:
  `OTGW-field-data/_comms/rest_cap_decision_rule.md`.
- Analysis: `OTGW-field-data/_comms/task3104_ac2_ac4.md` (sections 1 to 15); result note for Robert:
  `OTGW-field-data/_comms/task3104_ac6_result_note_draft.md`; implementation spec:
  `OTGW-field-data/_comms/task3104_impl_spec_adr300.md`.
- Bench files: `OTGW-field-data/bench-63/2026-11-26_rest-study/sweep_ble_on4/analysis_series4b_boot_b.txt` (sha256
  7fe10a91...), `analysis_series4c_ble_off.txt` (sha256 ec1a5b2f...), `sweep_ble_off/`, `sweep_ble_on3/`.
- Source: `src/OTGW-firmware/restAPI.ino:43-45`, `:76-93`, `:111-130`, `:2668`, `:2814-2830`, `:4173-4178`;
  `src/OTGW-firmware/data/v2.js:14-27`; prerelease rule `restGateDiag.h` and `restAPI.ino:107-110` at 3c59326af
  (otgw-dev lane, not on `dev`).
