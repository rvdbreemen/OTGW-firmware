# Optimal parallelism study — v2 Web UI / REST on ESP32-S3

Date: 2026-07-06
Status: Design approved, not yet executed
Branch: dev (2.0.0)
Epic task: TASK-1015

## Problem

The SAT onboarding fix (TASK-1014) forced client single-flight: the frontend fires at
most ONE `/api` call at a time, and page assets load sequentially. This is safe but
maximally conservative. The REST backpressure gate (TASK-884, `restInFlight`) caps
concurrent `/api` at 4 and clamps toward 1 under heap pressure; the file-serve gate
(ADR-147, `webFileInFlight`) caps static serves at 6→1. The real device ceiling is
unknown. Goal: measure the optimum parallelism (client offered-concurrency + gate caps)
that stays robust under combined real-world load, then bake it in.

## Objective (robustness-first)

Pick the HIGHEST parallelism N where every scenario (including overload and combined
stress) shows: zero crashes/hangs, no permanent gate clamp-to-1, `max_free_block` stays
above the heap-tier safety floor, and WS + HTTP recover after load. Tiebreak among safe
N: lower page-load latency.

## Target hardware

Classic-S3 on COM8 (MAC ac:27:6e:ce:45:d8), `esp32-classic` build (Classic + LOLIN S3
Mini + PIC). Must be WiFi-provisioned (SoftAP, maintainer step) with a known IP, telnet
(23), and WS reachable before any test runs. Second target (bench-otgw32) optional
cross-check later.

## Method — two-phase (approach C)

### Phase 1 — capacity curve
One instrumented firmware build with a BOUNDED safety gate (caps = 8 — above all test-N,
still sheds a true runaway so the device can never be bricked). Sweep offered-concurrency
in software 1 → 2 → 3 → 4 → 6 → 8 (no reflash per step; the load harness controls it).
Produce a curve of latency + incidents + firmware high-watermarks vs offered-N. This
reveals the true device ceiling independent of the shipped gate policy.

### Phase 2 — policy confirmation
Candidate N* = highest offered-N from Phase 1 with zero incidents. Set BOTH gate caps = N*
AND client `MAX_INFLIGHT` = N*, flash, run all scenarios incl. combined-stress + overload.
Holds → bake in. Fails → drop to N*-1 and re-confirm.

## Components

1. **Firmware instrumentation** (new, resettable via telnet `z`): `rest_inflight_hwm`,
   `webfile_inflight_hwm`, active lwIP TCP pcb count (`tcp_active_pcbs`), per-window
   `rest_503` / `webfile_503`, `min_max_block`, `min_free_heap`, `max_loop_gap_ms`.
   Exposed via `/api/v2/stats` (JSON) + telnet dump. Caps driven by build-flags
   `-DREST_MAX_INFLIGHT=N` / `-DWEB_FILE_MAX_INFLIGHT=N` (already `#ifndef`, set per arm in
   platformio.ini).
2. **Client concurrency knob**: single JS constant `MAX_INFLIGHT` driving the
   reduce-chain / asset loader (default 1 = TASK-1014 behaviour).
3. **Load harness** (Python asyncio + httpx): semaphore-controlled offered-concurrency,
   mix of `/api/v2` GETs (device/info, sat/status, health) + PUTs (settings-save pattern)
   + static assets (v2.html, v2.css, v2.js, ds-tokens.css). Logs status, latency, peak
   client open sockets.
4. **Browser page-load harness** (CDP headless-Edge): cold (empty cache) + warm (304)
   loads by IP. Measures total page-load, per-asset parallel connections,
   DOMContentLoaded, 503 count in the network log.
5. **Combined-stress orchestrator**: simultaneous browser page-load + API polling + WS
   live-log subscriber + OT traffic via `sat_boiler_emulator.py` (scenario S-combined).
6. **Metrics collector + safety**: per-arm `/api/v2/stats` poll + telnet `z` reset +
   crashlog watch, all through `capture-mqtt-debug.bat` (one transcript per arm). Watchdog:
   telnet unreachable / crashlog present / heap below floor → abort arm, log incident,
   esptool recovery ready. Never run the gate wide open (bounded 8 in Phase 1).

## Scenarios per arm
S1 cold page-load · S2 warm page-load · S3 API burst · S4 combined-stress ·
S5 overload (offer 2×N to exercise the 503 shed + recovery).

## Metrics per arm
page-load p50/p95, peak parallel connections (client + firmware hwm), 503 count,
min max-block, min free heap, max loop-gap, WS/HTTP drops, crashes/hangs.

## Deliverable
Bake N* into `REST_MAX_INFLIGHT`, `WEB_FILE_MAX_INFLIGHT`, and client `MAX_INFLIGHT`.
Write an ADR (method + data + chosen N; supersedes the pure single-flight rule of
TASK-1014). Bump prerelease + commit. Update CLAUDE.md single-flight rule to "at most N".

## Non-goals
- No HTTPS/WSS. No change to the gate ARCHITECTURE (only its cap values).
- No production push until Phase 2 confirms N* and the maintainer signs off.

## Risks
- Device not provisioned → whole study blocked (Phase 0 gate).
- esp32-classic differs from esp32-otgw32 (PIC vs OTDirect, different heap headroom);
  result may need cross-check on the bench-otgw32 before generalising.
- Combined-stress can brick the device (pcb exhaustion, ADR-147 class); watchdog +
  bounded gate + esptool recovery mitigate.
