---
id: TASK-3091
title: >-
  REST in-flight cap: measure the real maximum (N sweep 2..8) and raise
  REST_MAX_INFLIGHT if the evidence allows (Robert, priority)
status: In Progress
assignee:
  - '@otgw-poc-dev'
created_date: '2026-11-25T23:22:00Z'
updated_date: '2026-11-26T00:02:00Z'
labels:
  - rest
  - heap
  - bench
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1160000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert (2026-11-26 00:50): 'REST in-flight cap is 2 (REST_MAX_INFLIGHT, restAPI.ino:43, ADR-165 N*=2): can it go higher now, and find the realistic maximum to reduce or prevent the HTTP 503s'. Premise to keep straight: Nagle (EMC_TCP_NODELAY, ADR-266) touches only the espMqttClient socket, NOT the AsyncTCP web server, so it does not raise the web cap by itself; what could allow more: the web send caps ADR-188/216/240, the MQTT pacer (ADR-265) lowering concurrent heap spikes, ADR-295 recovery, AsyncTCP patch ADR-232. Measure, do not assume. DECISION RULE (draft, to be confirmed by the architect and the coordinator BEFORE the first run): the new default is the largest N in 2..8 that, on .61 (no PSRAM, the binding unit), in >= 3 repeats of >= 60 s per arm, shows (1) 0 reboots or crashes and 0 WS or MQTT disconnects, (2) hd_min_free_heap not below 8192 B (the ADR-030 ladder) and not more than 25 percent below the N=2 arm, (3) p99 latency not above 2x the N=2 arm, and (4) at least 30 percent fewer client-visible 503 than the N=2 arm; if no N passes (4) the default stays 2 and the evidence says why. The client-side rule 'max 2 API calls in flight' (CLAUDE.md core rules, ADR-184 UI queue) does NOT change with N unless the decision says so; state it explicitly in the result. Account for the load script's own effect on the device (perturbation): record its own request rate and run a no-load control arm.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A prerelease-only runtime override of the cap exists: debug-flag table entry 'restcap' (REST name and telnet key), default 2 = unchanged, range 2..8, RAM-only, under #if defined(_VERSION_PRERELEASE), one separate commit, host test red/green plus a mutant, the stable keeps the compile-time default 2
- [ ] #2 The decision rule above is confirmed by the architect and the coordinator and recorded in this task BEFORE the first measurement run
- [ ] #3 A sweep script extends scripts/loadtest_sweep.py (TASK-1015/1022/1024): N parallel requests for N = 2..8, per arm >= 60 s and >= 3 repeats, a no-load control arm, counting 503, client-visible failures, p95 and p99 latency, hd_min_free_heap, hd_min_max_block, reboots or crashes, WS and MQTT survival, whether the web UI stays usable, and the load script's own request rate
- [ ] #4 Bench report on .63 AND .61 (heap-limited only there) with the numbers per arm, evidence under OTGW-field-data with SHA256SUMS
- [ ] #5 Decision recorded: a new default N (ADR-165 superseded by a new ADR through adr-kit with the data) or 'stay at 2' with the evidence; the web UI queue (ADR-184, 2 in flight) follows the decision and the result states whether it changes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-26 otgw-poc-dev: part 1 (the prerelease runtime override) is done locally on lane-poc-restcap (from origin/dev 6ece06e03): DebugSection.iRestCap (default 2), restCapBase() in restAPI.ino (prerelease: state.debug.iRestCap clamped to 2..8; stable: REST_MAX_INFLIGHT), the heap-tier clamp stays (maxblk >= 24000 full cap, 16000..24000 cap 2, below 1), REST name restcap (POST /api/v2/debug/flags {"name":"restcap","value":"6"}, GET lists it) and telnet key R (cycles 2..8), both prerelease only and RAM only. Note: the debug-flag table is bool-typed, so restcap sits next to it (debugRestCapParse / debugRestCapNext in debugFlags.h) instead of in kDebugFlags. Host evidence: tests/test_rest_cap_override.py 9/9 (real cap code and parse helpers compiled with and without _VERSION_PRERELEASE; red on origin/dev: 2 where 6 is expected; mutants override-ignored, clamp-removed, stable-honours-override red), test_debug_flags_api 8, test_evaluate 80. Fact for the sweep: the effective cap is min(override, heap tier): at maxblk below 24000 the cap is 2 whatever the override says, so on .61 (no PSRAM, maxblk often below 24000) raising N can have no effect; the sweep must record the effective cap and maxblk per arm (restAPI.ino logs 'REST BUSY ... cap'). The existing scripts/loadtest_sweep.py sweeps OFFERED concurrency against a fixed gate of 8; this task needs the GATE set per arm through restcap while the offered load is the real client rule (2 per browser, ADR-184). NOT done: the sweep script extension, the confirmed decision rule, any bench run, the ADR. Robert redirected this agent to the remote telemetry PoC (2026-11-26 ~02:00): the remaining parts (AC 2 to 5) go back to the coordinator.

2026-11-26 otgw-webui (AC3): scripts/loadtest_sweep.py --restcap + tests/test_loadtest_restcap_sweep.py (26 host tests against a fake device with the firmware gate; 11 of 11 meaningful mutants killed). Sets the gate per arm through restcap (readback, else the arm ends with cap_not_applied and offers no load), fixed offered load (--offered, default 8), arms 2..8 plus a no-load control, rounds with rotating arm order, per repeat: 503, other failures, p50/p95/p99, bootcount, mqtt-down samples, hd_ws_disconnects, optional /ws client, hd_min_free_heap before/after, 1 Hz freeheap and maxfreeblock, the EFFECTIVE cap (min(override, heap tier)), /v2.html probe under load, own request rate; telnet z reset per repeat; abort below freeheap 6000; cap back to 2 in finally; sweep.json + SHA256SUMS. Decision rule = RULE + evaluate_decision() in the script. Proposed additions to the draft (architect to confirm or drop, AC2): (1) offered load fixed above N, because rule 4 is unmeasurable when offered equals N; (2) the 25 percent heap comparison on the sampled freeheap minimum, since hd_min_free_heap is an all-time low nothing resets (watermark keeps the 8192 floor); (3) at least 20 good samples per repeat (the sampler shares the REST gate); (4) N counts only if the effective cap was N in at least 95 percent of the samples. Not run on a device yet.
<!-- SECTION:NOTES:END -->
