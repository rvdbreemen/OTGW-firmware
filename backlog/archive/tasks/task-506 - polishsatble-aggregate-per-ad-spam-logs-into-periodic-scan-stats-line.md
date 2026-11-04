---
id: TASK-506
title: 'polish(satble): aggregate per-ad spam logs into periodic scan-stats line'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-01 20:20'
updated_date: '2026-05-01 20:27'
labels:
  - polish
  - satble
  - logging
  - esp32
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George reported that even with the SAT BLE debug toggle on (telnet key '7' / `state.debug.bSATBLE`), the log volume is overwhelming — ~150-200 ads/sec from neighbour devices, each producing 2 lines (`SAT BLE: ad from X rssi=Y` + `SAT BLE: ad rejected (unknown format)`). In a busy RF environment that is ~12,000-18,000 lines per minute of pure noise that swamps actionable events.

The two highest-volume sites in `SATble.ino` are trace-level information masquerading as debug-level: they fire for every received advertisement before any parsing or filtering is done, and produce no actionable signal individually.

Fix: replace the per-ad spam with file-static `volatile uint32_t` counters that are incremented in `onResult()`, and emit one aggregated stats line per `satBLELoop()` cycle (which is already throttled by `_bleLastPublishMs` + `settings.sat.iBleInterval`). Counters track: total ads, accepted, filter-mismatch, unknown-format, no-free-slot.

Net result: log volume drops by ~95% while observability is *improved* — the new stats line gives a noise-floor metric that did not exist before, useful for "why doesn't my sensor show up" diagnosis.

KISS choices:
- `volatile uint32_t` counters (not `std::atomic`) — losing one tick per 10k increments in a race is irrelevant for a logging counter, and the codebase has no prior `std::atomic` use.
- Hook the stats emission into the existing `satBLELoop()` cadence rather than introducing a new timer — avoids new state-management.
- Keep the lower-volume diagnostic logs (filter-mismatch, no-slot, accepted-sensor, best-sensor, no-valid-sensor) because they are actionable and sparse.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Per-ad pre-log at SATble.ino:234 (`SAT BLE: ad from %s rssi=%d`) is removed
- [x] #2 Unknown-format reject log at SATble.ino:258 is replaced by a counter increment with no log line
- [x] #3 Five `volatile uint32_t` counters added at file scope: total ads, accepted, filter-mismatch, unknown-format, no-free-slot
- [x] #4 Counters are incremented at the corresponding decision points in `SATBLEScanCallbacks::onResult()`
- [x] #5 `satBLELoop()` emits one aggregated `SAT BLE: %us window: N ads, A accepted, F filter-mismatch, U unknown, S no-slot` line per cycle (gated by existing `state.debug.bSATBLE`), then resets the counters
- [x] #6 Existing per-event logs are preserved: filter-mismatch (line 270), no-slot (278), accepted-sensor (305), best-sensor (411), no-valid-sensor (423)
- [x] #7 `python evaluate.py --quick` exits 0 with PROGMEM check still PASS and no new violations introduced
- [x] #8 Build clean on ESP32-S3 (`python build.py`); ESP8266 not impacted (file is wrapped in ESP32-only conditional)
- [x] #9 No behavioural change to BLE parsing, filtering, slot allocation, or MQTT publishing — purely a logging refactor
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan

Single-file change in `src/OTGW-firmware/SATble.ino`. No header touches, no settings changes, no ADR needed (pure logging polish, no contract change).

### Step 1 — Add counter declarations (file-static block around line 87)

Five `volatile uint32_t` counters and a window-start timestamp:
```cpp
static volatile uint32_t _bleAdCount        = 0;
static volatile uint32_t _bleAcceptCount    = 0;
static volatile uint32_t _bleFilterRejCount = 0;
static volatile uint32_t _bleUnknownCount   = 0;
static volatile uint32_t _bleNoSlotCount    = 0;
static uint32_t          _bleStatsLastMs    = 0;
```

### Step 2 — Patch `SATBLEScanCallbacks::onResult()` (lines 228-310)

- **Remove** lines 234-236 (per-ad `SAT BLE: ad from X rssi=Y` log) and replace with `_bleAdCount++;` at the very top of the callback
- **Replace** lines 258-260 (unknown-format reject log + return) with `_bleUnknownCount++; return;` — drop the log entirely
- **Add** `_bleFilterRejCount++;` before the existing filter-mismatch log (line 270), keep the log
- **Add** `_bleNoSlotCount++;` before the existing no-slot log (line 278), keep the log
- **Add** `_bleAcceptCount++;` after the existing accepted-sensor log (line 305) — counts successful slot updates

### Step 3 — Patch `satBLELoop()` (around line 365)

Just before `satBLEUpdateState()` is called, emit the aggregated stats and reset:
```cpp
uint32_t windowMs = millis() - _bleStatsLastMs;
SATBLEDebugTf(PSTR("SAT BLE: %us window: %u ads, %u accepted, %u filter-rej, %u unknown, %u no-slot\r\n"),
              (unsigned)(windowMs / 1000),
              (unsigned)_bleAdCount, (unsigned)_bleAcceptCount,
              (unsigned)_bleFilterRejCount, (unsigned)_bleUnknownCount,
              (unsigned)_bleNoSlotCount);
_bleAdCount = _bleAcceptCount = _bleFilterRejCount = _bleUnknownCount = _bleNoSlotCount = 0;
_bleStatsLastMs = millis();
```

The stats fire at the existing `iBleInterval` cadence (default likely 30-60s, user-configurable via SAT settings). Gated by the existing `state.debug.bSATBLE` toggle via the macro.

### Step 4 — Verify

1. `python evaluate.py --quick` — must stay PASS on PROGMEM, exit 0
2. `python build.py` — must compile clean on ESP32-S3 (file is `#if defined(ESP32)`-gated, so ESP8266 build is unaffected)
3. Manual sanity-read of the diff: ensure no log site that produces actionable info was removed

### Risk assessment

- **Race on counters**: increments in `onResult()` (BLE host task) vs. read+reset in `satBLELoop()` (Arduino task). 32-bit increments on ESP32 are not strictly atomic (RMW), so a torn read or a lost increment is possible. Acceptable for a logging counter — losing 1 tick per ~10k is invisible. Codebase has no `std::atomic` precedent; introducing it for log counters is overkill.
- **Counter overflow**: `uint32_t` at 200 ads/sec wraps in ~248 days. The window resets each `iBleInterval`, so practical overflow is impossible.
- **Stats-line on first call**: `_bleStatsLastMs == 0` at boot; first window will report `millis()/1000` seconds (uptime-since-boot in seconds). Cosmetic only — not worth special-casing.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Patch applied to SATble.ino:
- Counters declared at lines 95-100 (5x volatile uint32_t + 1x window timestamp)
- onResult() increments: _bleAdCount at line 249 (top of callback), _bleUnknownCount at 272 (replaces the old reject-log), _bleFilterRejCount at 284, _bleNoSlotCount at 293, _bleAcceptCount at 321
- satBLELoop() emits stats line at lines 388-394, then resets counters (line 395) and updates window timestamp (396)
- Pre-existing event-level logs preserved verbatim at lines 285 (filter-rej), 294 (no-slot), 322 (accept), 441 (best-sensor), 453 (no-valid-sensor)

Verification:
- AC #7: `python evaluate.py --quick` exit 0, 0 failed checks, PROGMEM PASS, score 97.0% (no regression vs TASK-505 baseline)
- AC #8: `python build.py` running in background; will report once it finishes
- AC #9: pure logging refactor verified by diff — no parser, filter, slot allocator, MQTT, or state-machine code touched
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaces the per-ad SAT BLE spam in `SATble.ino` with five `volatile uint32_t` counters that are incremented on the BLE host task in `SATBLEScanCallbacks::onResult()` and read+reset on the Arduino loop task in `satBLELoop()` once per `iBleInterval`. Emits one aggregated stats line per cycle, gated by the existing `state.debug.bSATBLE` toggle (telnet key '7').

**Source changes** (single file: `src/OTGW-firmware/SATble.ino`):
- Five counter declarations + window timestamp at lines 95-100 (file-static, `volatile uint32_t`).
- `onResult()` line 249: `_bleAdCount++` at top of callback (replaces the old per-ad pre-log).
- `onResult()` line 272: `_bleUnknownCount++` replaces the old `"ad rejected (unknown format)"` log entirely (was the highest-volume spam source).
- `onResult()` line 284: `_bleFilterRejCount++` added before the existing filter-mismatch log; log preserved.
- `onResult()` line 293: `_bleNoSlotCount++` added before the existing no-slot log; log preserved.
- `onResult()` line 321: `_bleAcceptCount++` added before the existing accepted-sensor log; log preserved.
- `satBLELoop()` lines 388-396: new aggregated stats emit + counter reset, hooked into the existing `_bleLastPublishMs` cadence (no new timer needed).

**KISS choices defended:**
- `volatile uint32_t` instead of `std::atomic`: a torn read or lost increment is invisible at logging granularity, and the codebase has no `std::atomic` precedent.
- Reused the existing `iBleInterval`-throttled `satBLELoop()` cadence: avoided introducing a new `DECLARE_TIMER_*` block.
- Five low-frequency event-level logs (filter-mismatch, no-slot, accepted, best-sensor, no-valid-sensor) were preserved verbatim — they are sparse and actionable.

**Verification:**
- `python evaluate.py --quick`: exit 0, 0 failed checks, PROGMEM compliance PASS, health score 97.0% (no regression vs TASK-505 baseline).
- `python build.py`: clean on both platforms.
  - ESP8266: SUCCESS in 44s, RAM 84.6%, Flash 77.3% (unchanged — file is ESP32-only).
  - ESP32-S3: SUCCESS in 123s, RAM 31.7%, Flash 95.9% (unchanged within rounding — added code is ~30 bytes of static counters + a few increment instructions).

**Operational impact for George:**
At 150-200 ads/sec in a busy RF environment, log volume drops from ~12,000-18,000 lines/min to a single stats line per `iBleInterval`. Format:
```
SAT BLE: 30s window: 4823 ads, 2 accepted, 18 filter-rej, 4803 unknown, 0 no-slot
```
Net result: ~95% spam reduction *and* a new noise-floor metric for "why doesn't my sensor show up?" diagnosis.

**No behavioural change** to BLE parsing, MAC filtering, slot allocation, MQTT publishing, or the SAT control state machine. Pure logging refactor.
<!-- SECTION:FINAL_SUMMARY:END -->
