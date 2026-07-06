---
id: ADR-165
title: "Optimal request parallelism on ESP32-S3 v2 Web UI/REST: N*=2 confirmed by two-phase load test (TASK-1015)"
status: Accepted
date: 2026-07-06
tags: [esp32s3, rest-api, webui, concurrency, backpressure, load-test, task1015, task1014, task884, task1017]
supersedes: []
superseded_by: []
related: [ADR-149, ADR-147, ADR-145, ADR-089]
deciders: [Robert van den Breemen]
---

# ADR-165: Optimal request parallelism on ESP32-S3 v2 Web UI/REST: N*=2 confirmed by two-phase load test (TASK-1015)

## Status

Accepted. Date: 2026-07-06.

**Accepted by explicit maintainer review** (Robert van den Breemen, 2026-07-06,
in-session sign-off after reading this ADR). N*=2 is now the confirmed
production ceiling for `REST_MAX_INFLIGHT` and `WEB_FILE_MAX_INFLIGHT` on the
`esp32-classic` target. The three bake-in actions previously deferred (see
Decision) are executed as part of accepting this ADR: the production
`build_flags` defaults are lowered to 2, and `CLAUDE.md`'s single-flight rule
is updated to reference this hard cap. A client-side `MAX_INFLIGHT` JS knob is
NOT implemented — the current frontend's single-flight discipline (N=1) is
already within the new N=2 ceiling, so no client change is required to honor
this cap; that knob remains unbuilt future work if throughput ever motivates
raising client concurrency above 1 (bounded at 2 by this ADR either way).

**Cross-target scope note (unchanged by acceptance):** this ADR's data covers
only `esp32-classic`. Generalizing N*=2 to `esp32-otgw32` (OTDirect, different
heap headroom profile) remains unverified, per the Risks section below.

## Status History

status_history:
  - date: 2026-07-06
    status: Proposed
    changed_by: Agent (TASK-1026)
    reason: Initial decision record documenting the TASK-1015 two-phase parallelism load-test study (method, data, chosen N*=2). Left Proposed per the study's own non-goal ("no production push until the maintainer signs off").
    changed_via: adr-kit
  - date: 2026-07-06
    status: Accepted
    changed_by: Robert van den Breemen
    reason: "Read the ADR, approved N*=2 as the new hard limit. Directed the bake-in: lower REST_MAX_INFLIGHT/WEB_FILE_MAX_INFLIGHT production defaults to 2 and update CLAUDE.md's single-flight rule to document the cap. No client-side MAX_INFLIGHT knob requested (current N=1 client discipline is already within the new N=2 ceiling)."
    changed_via: adr-kit

## Context

The SAT (Smart Access Thermostat) onboarding fix (TASK-1014) forced the v2 Web
UI frontend into strict client single-flight: the frontend fires at most ONE
`/api` request at a time, and page assets load sequentially (documented as a
binding project rule in `CLAUDE.md`, "Single-flight API calls (NEVER burst)").
This was the safe, conservative fix for a real bug (SAT onboarding silently
failing to save settings because bursts of concurrent `/api` calls tripped the
REST backpressure gate's cheap 503 response). It was never claimed to be
optimal, only safe.

On the firmware side, two independent backpressure gates already exist and
were sized by earlier, narrower experiments, not by a systematic sweep:

- The REST request-inflight gate, `REST_MAX_INFLIGHT` (default 4,
  `src/OTGW-firmware/restAPI.ino:44`), heap-tier-aware via
  `restEffectiveInflightCap()` (ADR-149, grounded in the TASK-884
  connection-flood experiment).
- The static-file-serve gate, `WEB_FILE_MAX_INFLIGHT` (default 6,
  `src/OTGW-firmware/restAPI.ino:80`), from ADR-147 (esp32-classic and
  esp32-otgw32 platform limits for concurrent LittleFS static serving).

The real device ceiling for offered concurrency, and whether the client's
single-flight discipline (N=1) was leaving usable headroom on the table or
whether the shipped gate caps (4 / 6) were already too generous for the
esp32-classic bench target, was unmeasured. TASK-1015 was opened as an epic to
measure it empirically with a repeatable, two-phase method and then propose
(not yet bake in) a chosen N.

The full approved method is `docs/superpowers/specs/2026-07-06-parallelism-loadtest-design.md`
(status: "Design approved, not yet executed" at spec-write time; both phases
have since executed under TASK-1022 and TASK-1024).

## Decision

**Adopt N*=2 as the confirmed safe-and-robust parallelism ceiling for
`REST_MAX_INFLIGHT` and `WEB_FILE_MAX_INFLIGHT` on the ESP32-S3
`esp32-classic` bench target (Classic + LOLIN S3 Mini + PIC, COM8, MAC
`ac:27:6e:ce:45:d8`), confirmed by a two-phase load test (Phase 1 capacity
curve, TASK-1022; Phase 2 policy confirmation, TASK-1024) run against the v2
Web UI / REST stack.**

### Method (as executed)

**Phase 1 — capacity curve (TASK-1022).** One instrumented `esp32-classic`
build with `REST_MAX_INFLIGHT=8` / `WEB_FILE_MAX_INFLIGHT=8` (a bounded safety
gate deliberately set above every offered-N under test, so the arm can shed a
true runaway without ever bricking the device). A Python asyncio/httpx load
harness (`scripts/loadtest_sweep.py`) swept client offered-concurrency in
software across 1, 2, 3, 4, 6, 8 (no reflash between steps), running scenarios
S1 (cold page-load), S2 (warm page-load), S3 (API burst), S4
(combined-stress: browser page-load + API polling + WS live-log + synthetic OT
traffic), and S5 (overload, offered = 2xN) per step. Firmware instrumentation
(TASK-1017 counters, exposed via `/api/v2/stats` and telnet) recorded
`rest_inflight_hwm`, `webfile_inflight_hwm`, per-window `rest_503` /
`webfile_503` counts, `min_max_block`, `min_free_heap`, and `max_loop_gap_ms`.

**Phase 2 — policy confirmation (TASK-1024).** Set both gate caps AND the
load harness's offered concurrency to the candidate N*, flash, and re-run the
full S1-S5 set including combined-stress and overload, watching for zero
incidents (no crash, hang, or heap-floor breach), heap-tier safety, and WS/HTTP
recovery after load.

### Data

Phase 1 capacity curve (esp32-classic, bounded gate cap=8, 8 scenario-seconds
per arm; `rest_503`/`webfile_503` are the per-arm S1-S5 totals, "nominal-load
S3 503" isolates just the non-overload API-burst scenario):

| offered-N | min_max_block | max_loop_gap_ms | rest_503 (total) | webfile_503 (total) | nominal-load S3 503s |
|---|---|---|---|---|---|
| 1 | 8692 | 779 | 1 | 6 | 0 |
| 2 | 7668 | 491 | 5 | 19 | 0 |
| 3 | 9204 | 579 | 26 | 39 | 3 |
| 4 | 7668 | 742 | 85 | 80 | 31 |
| 6 | 7668 | 644 | 84 | 69 | 35 |
| 8 | 6644 | 613 | 143 | 92 | 69 |

Zero watchdog incidents (no crash, hang, or heap-floor breach) were observed
at every offered-N in {1,2,3,4,6,8}; the bounded safety gate (cap=8) held
across the whole tested range, confirming the Phase 1 safety design worked as
intended. S5 (overload, offered=2xN) correctly shed load with 503s from N=2
onward and the device recovered every time.

However, a second and, for this ADR's purpose, decisive signal diverges from
the "zero incidents" criterion: **503s under nominal (non-overload) load start
appearing at N=3 and climb steeply** (0, 0, 3, 31, 35, 69 nominal-load S3 503s
at N=1 through N=8 respectively). Above N=2, real users would see visible
request failures during ordinary browsing, well before the device is in any
danger of crashing. N=1 and N=2 are the only two arms with zero nominal-load
503s.

Per the literal AC wording of TASK-1022 ("candidate N* = highest offered-N
with zero incidents"), the crash/hang/heap-floor-breach criterion alone would
select N*=8. That reading was flagged to, and overridden by, the maintainer:
the nominal-load 503 climb is real user-visible failure, not a benign
edge-case, so it was treated as the decisive robustness signal for choosing
the confirmation candidate rather than raw survivability. The maintainer
directed Phase 2 to skip the spec's literal N=8-down-to-2 confirmation walk
(which would have cost roughly 6 build/flash/test cycles for arms already
known unfit) and confirm the last 503-clean arm, N=2, directly.

Phase 2 confirmation (esp32-classic, `REST_MAX_INFLIGHT=2` /
`WEB_FILE_MAX_INFLIGHT=2`, client offered concurrency matched to 2, single
confirmation arm covering S1-S5 including S4 combined-stress and S5 overload
at offered=4=2xN):

- Zero watchdog incidents (no crash, hang, or heap-floor breach).
- WebSocket stayed connected throughout (18 frames observed); OT onboard-sim
  toggle confirmed enable and disable.
- S1/S2 page-loads: 0 503s cold and warm.
- S3 nominal-load API burst: 2/80 503s (attributed to scenario-launch overlap,
  not steady-state pressure).
- S5 overload (offered=4): 43/144 requests correctly shed with 503, device
  fully recovered afterward.
- Heap high-water marks: `min_max_block=9204`, `min_free_heap=14816` (healthy
  headroom, well clear of the heap-tier safety floor stressed at higher N in
  Phase 1), `max_loop_gap_ms=588`, `tcp_active_pcbs=6`.

**N*=2 holds.** No further reduction to N*-1 was required.

### What this ADR documents

- The chosen ceiling: `REST_MAX_INFLIGHT=2` and `WEB_FILE_MAX_INFLIGHT=2` are
  the empirically confirmed values for the `esp32-classic` bench target under
  the tested scenario mix.
- The client-side offered concurrency used in Phase 2's confirmation (matching
  the gate caps at 2) is the counterpart value a future client-side
  `MAX_INFLIGHT` knob would need, if and when such a knob is implemented (see
  below — it does not exist yet).

### Bake-in status (post-acceptance)

At Proposed status, this ADR explicitly withheld authorization for three
follow-up actions pending maintainer review. Following acceptance
(2026-07-06), the maintainer authorized two of the three; the third remains
deliberately unbuilt:

1. **Production `build_flags` defaults — DONE.** `src/OTGW-firmware/restAPI.ino`'s
   `REST_MAX_INFLIGHT` (was 4, line 44) and `WEB_FILE_MAX_INFLIGHT` (was 6,
   line 80) are lowered to `2`, matching the Phase 2-confirmed N*. This is now
   the hard production ceiling for both gates on `esp32-classic`.
2. **Client-side `MAX_INFLIGHT` concurrency knob — NOT built, by choice.** No
   such JS constant was added to `v2.js`. The current frontend's single-flight
   discipline (N=1, from TASK-1014) is already within the new N=2 ceiling, so
   no client change was needed to honor this ADR's cap. This remains available
   future work if throughput ever motivates raising client-offered
   concurrency (bounded at 2 by this ADR regardless).
3. **`CLAUDE.md`'s single-flight rule — DONE.** Updated from a flat
   "single-flight (NEVER burst)" statement to document the confirmed N<=2 hard
   cap, citing this ADR.

### Tooling bugs found and fixed during the study (not firmware defects)

Two real bugs surfaced in the *host-side load-test harness* while producing
the Phase 1 data; both were fixed before the "clean" Phase 1 run (v3) that
produced the table above. Neither is a firmware defect:

1. **`/api/v2/device/info` field nesting.** The endpoint nests all fields
   under a top-level `"device"` key. The harness's `hwm_snapshot()` / watchdog
   code initially read fields at the top level and silently got `None` /
   `false`, producing false readings rather than an error.
2. **Non-resettable heap watermark used as a live signal.** The harness's
   safety watchdog originally checked `hd_min_free_heap` for a heap-floor
   breach. That field is the ESP32 allocator's all-time-low watermark and is
   **not resettable** (unlike the other TASK-1017 counters, which telnet `z`
   does reset). Once it dipped once in an earlier run, it stayed low for the
   rest of the session, so the watchdog false-aborted every subsequent arm at
   roughly 5.7 seconds, before a transcript had time to form. Fixed by polling
   `freeheap` (the current, live free-heap value) instead of the historical
   low-watermark.

## Alternatives Considered

### Alternative A: Keep the current shipped defaults (REST=4, WEB_FILE=6) and the client single-flight rule (N=1) unchanged

The status quo: gate caps sized by the earlier, narrower TASK-884/ADR-147
experiments, client discipline forced to N=1 by the TASK-1014 SAT bug fix.

Rejected as the target state (though it remains the *actual* shipped state
until maintainer sign-off, per the "what this ADR does not yet authorize"
section above). The Phase 1 curve shows N=1 leaves usable headroom: N=2
produces zero nominal-load 503s and zero incidents, with headroom nearly
identical to N=1 (`min_max_block` 7668 vs 8692, both comfortably above the
heap-tier floor) and materially lower request latency potential from allowing
2-way pipelining instead of forcing full serialization. Never testing whether
N=1 was overly conservative would leave that headroom permanently unclaimed.

### Alternative B: Adopt N*=8 per the literal "zero incidents" AC wording

TASK-1022's AC literally reads "candidate N* = highest offered-N with zero
incidents (crash/hang/heap-floor breach)." By that reading alone, N=8 wins:
the bounded safety gate held at every tested N, including 8, with no crash or
heap-floor breach anywhere in the sweep.

Rejected. Survivability under the bounded safety gate is not the same as
usability under normal traffic. At N=8, nominal (non-overload) load already
produces 69 API 503s in an 8-second window (versus 0 at N=1-2) — real users
polling the dashboard or saving settings would see visible request failures
during ordinary browsing, not just during a synthetic overload scenario. The
maintainer explicitly overrode the literal AC reading in favor of the
nominal-503-clean ceiling once this divergence was flagged (see Data section
above). A device that never crashes but constantly 503s its own users is not
the robustness the epic (TASK-1015) was chasing.

### Alternative C: Run the full literal Phase 2 walk (N=8 confirm, fail, N=6, fail, N=4, fail, N=3, fail, N=2, confirm)

The spec's literal method describes confirming starting at the Phase 1
candidate and stepping down by 1 (or to the next tested arm) until an arm
passes. Applied literally with candidate=8, this would cost roughly 6
build/flash/test cycles (one per arm from 8 down to 2) before reaching the
same N*=2 answer.

Rejected as wasteful given the Phase 1 data already available. The Phase 1
capacity curve had already shown, in one sweep with no reflash between steps,
exactly where the nominal-503-clean ceiling sits (N=2). Re-deriving that one
build/flash cycle at a time when the data pointed straight at the answer would
have spent five extra flash/test cycles to reconfirm a boundary the curve
already located. The maintainer authorized skipping directly to confirming
N=2 (still running the FULL Phase 2 scenario set, including combined-stress
and overload, at that single candidate — only the *walk*, not the
*confirmation depth*, was shortened).

### Alternative D: Do nothing (no study)

Leaving the gate caps and the client single-flight rule unmeasured was the
state prior to TASK-1015. Rejected: TASK-1014's single-flight fix was a
correctness patch for a real bug, made under time pressure, with no claim of
being tuned for throughput. Without a systematic sweep there was no way to
tell whether N=1 was leaving usable headroom on the table (it was: see
Alternative A) or whether the shipped gate defaults of 4/6 were already
riskier than believed for the esp32-classic target (they are: nominal 503s at
N=4 already run 31 in 8 seconds, well above the N=2 baseline of 0).

## Consequences

**Benefits**

- Produces the first systematic, repeatable capacity curve for offered
  concurrency on the `esp32-classic` ESP32-S3 target, covering six offered-N
  values across five load scenarios each, with firmware-side high-watermark
  and 503-count instrumentation (TASK-1017) rather than guesswork.
- Identifies a concrete, defensible ceiling (N*=2) with both a survivability
  justification (Phase 1: zero incidents across the full 1-8 range) and a
  usability justification (Phase 1: only N<=2 is nominal-503-clean; Phase 2:
  N=2 holds under combined-stress and overload).
- Surfaces and permanently fixes two host-side load-test tooling bugs
  (`device/info` field nesting; non-resettable heap-watermark misuse as a live
  signal) that would otherwise silently corrupt future load-test results run
  with the same harness.
- Leaves the door open for future re-measurement on the `esp32-otgw32`
  (OTDirect) target, which the design spec flags as a distinct heap-headroom
  profile from `esp32-classic` (PIC) and was explicitly out of scope for this
  pass.

**Trade-offs**

- Post-acceptance, the shipped `REST_MAX_INFLIGHT` and `WEB_FILE_MAX_INFLIGHT`
  are both lowered from their prior values (4 and 6 respectively) to 2. This
  is a stricter gate than before for any workload that was relying on
  offered concurrency above 2 slipping through without a 503 (there was no
  such supported workload — the client was already single-flight — but any
  future feature that fires more than 2 concurrent `/api` or file requests
  will now shed sooner than it would have under the old 4/6 caps).
- The study measured only the `esp32-classic` target. Generalizing N*=2 to
  `esp32-otgw32` (different heap headroom profile, OTDirect instead of PIC) is
  explicitly unverified; the design spec calls this out as a risk requiring a
  cross-check, not yet performed.
- Scenario duration per arm was short (8 scenario-seconds in Phase 1); longer
  soak behavior at N=2 (multi-hour heap-fragmentation drift, per the TASK-934
  heap-frag soak methodology) was not exercised by this study and remains
  unverified.

**Risks and mitigations**

- *Risk*: N*=2, measured only on `esp32-classic`, gets silently assumed to
  also hold on `esp32-otgw32`.
  *Mitigation*: the design spec's own Risks section already flags this
  cross-check as outstanding; a follow-up task should confirm or re-derive N*
  on `esp32-otgw32` before that target's build flags are changed to match.
- *Risk*: the load-test harness bugs (device/info nesting, non-resettable
  watermark) recur in a future load-test script that does not reuse
  `scripts/loadtest_sweep.py`.
  *Mitigation*: both bugs and their fixes are documented in this ADR's Context
  section and in TASK-1022's implementation notes, as a reference for anyone
  writing a new load-test harness against `/api/v2/device/info` or the
  TASK-1017 heap-diagnostics counters.

## Related Decisions

- **ADR-149 (Accept the LWIP TCP-pcb connection ceiling on the ESP32-S3)**:
  complements this ADR. ADR-149 grounds the existing REST inflight gate
  (`restInFlight` / `restEffectiveInflightCap()`) in the TASK-884
  connection-flood experiment and documents the application-level connection
  mitigations this study exercises under load without changing their
  architecture.
- **ADR-147 (ESP32-S3 platform limits for concurrent webui static-file
  serving)**: the `WEB_FILE_MAX_INFLIGHT` gate this study measures against was
  introduced by ADR-147. This ADR proposes a new cap value for that same gate;
  it does not revisit ADR-147's architecture.
- **ADR-145 (Serve REST JSON via a chunked, pull-based response)**: cited in
  the `restAPI.ino` comment trail as the deeper architectural fix for the
  whole-response-buffer issue that motivates the REST inflight gate in the
  first place (superseded by ADR-146, which reverted to hand-rolled streaming
  JsonEmit; the gate itself is orthogonal to that reversion).
- **ADR-089 (Heap tier-machine contract)**: `restEffectiveInflightCap()` and
  `webFileGateTryAdmit()` both consult the heap-tier machine
  (`platformMaxFreeBlock()`) to tighten their caps under pressure; this
  study's confirmed N*=2 sits inside, and does not change, that tier contract.
- **Supersedes the pure single-flight rule established by TASK-1014**
  (`CLAUDE.md`, "Single-flight API calls (NEVER burst)"). That rule was never
  itself the subject of a standalone ADR; this ADR is the first record of a
  measured alternative to it. `CLAUDE.md` is updated per the Bake-in status
  section above to document the confirmed N<=2 hard cap.

## References

- Design spec: `docs/superpowers/specs/2026-07-06-parallelism-loadtest-design.md`
- Epic: TASK-1015 ("Research optimal client/firmware parallelism for v2 Web UI (load-test study)")
- Phase 1: TASK-1022 ("Phase 1: capacity-curve sweep (offered-N 1..8, bounded gate 8)")
- Phase 2: TASK-1024 ("Phase 2: policy confirmation at N* (matched knobs)")
- Bake-in follow-up (not yet executed by this ADR): TASK-1026
- Load harness: `scripts/loadtest_sweep.py`
- Raw Phase 1 data: `build/phase1_sweep_result_v3.json`, per-arm transcripts under
  `build/loadtest-sweep-phase1-v3/`
- Gate implementation: `src/OTGW-firmware/restAPI.ino:27-98`
  (`REST_MAX_INFLIGHT`, `restEffectiveInflightCap()`, `WEB_FILE_MAX_INFLIGHT`,
  `webFileGateTryAdmit()`)
- Client single-flight rule: `CLAUDE.md`, "Single-flight API calls (NEVER burst)"
  (established by TASK-1014)
- Firmware instrumentation counters: TASK-1017 (`state.heapdiag.*` fields,
  `/api/v2/stats`)
- Espressif ESP32-S3 LWIP (lightweight IP stack) TCP pcb (protocol control block)
  pool background: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_netif.html
