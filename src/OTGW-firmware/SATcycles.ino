/*
***************************************************************************
**  Module   : SATcycles.ino
**  Description: Cycle tracker and classifier for SAT
**
**  Ported from SAT Python custom component (releases/thermo-nova) cycles/
**  Original SAT component by Alex Wijnholds (https://github.com/Alexwijn/SAT)
**  SAT concept and algorithm design by George Dellas
**
**  Monitors flame ON/OFF transitions, builds per-cycle metrics,
**  and classifies each completed cycle to drive PWM auto-switching.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

// --- Cycle Kind & Phase enums are in OTGW-firmware.h ---

// --- Cycle Constants ---
static const uint8_t  SAT_CYCLE_HISTORY_SIZE       = 16;
static const float    SAT_CYCLE_SHORT_DURATION_SEC  = 60.0f;   // Cycles shorter than this = SHORT_CYCLING
// Overshoot margin now read from settings.sat.fOvershootMargin (default 2.0C)
static const float    SAT_OVERSHOOT_SUSTAIN_SEC     = 60.0f;   // Sustained overshoot before PWM switch
static const float    SAT_UNDERSHOOT_MARGIN_C       = 2.0f;    // Below setpoint margin for underheat
static const float    SAT_UNDERHEAT_SUSTAIN_SEC     = 180.0f;  // Sustained underheat before continuous switch
static const float    SAT_SATURATION_SUSTAIN_SEC    = 300.0f;  // Sustained saturation before continuous switch
static const uint32_t SAT_DHW_OVERSHOOT_GUARD_MS    = 300000UL; // 300s guard: skip overshoot->PWM switch during/after DHW

// --- Per-hour cycle counter (rolling 60-minute window, Task #203) ---
// Max cycles/hour is capped at 6 by settings; ring buffer holds 6 timestamps.
static const uint8_t SAT_MAX_CYCLES_PER_HOUR = 6;
static uint32_t _hourCycleTs[SAT_MAX_CYCLES_PER_HOUR]; // millis() of each flame-on event
static uint8_t  _hourCycleHead  = 0;  // next write position
static uint8_t  _hourCycleCount = 0;  // valid entries (0..SAT_MAX_CYCLES_PER_HOUR)

// --- Rolling 4-hour cycle window (Task #227) ---
// 60 slots: covers 4h at a 4-min average cycle period, or 2h at 2-min average.
// Each record is ~24 bytes; 60 records = ~1.4 KB — acceptable on ESP8266.
static const uint8_t  SAT_WIN4H_SIZE    = 60;
static const uint32_t SAT_WIN4H_SPAN_MS = 4UL * 3600UL * 1000UL; // 4 hours in ms

struct SATWindowRecord {
  uint32_t endMs;          // millis() when flame went OFF (cycle end)
  uint32_t onDurationMs;   // flame-ON duration for this cycle
  uint32_t offDurationMs;  // flame-OFF gap that preceded this cycle
  float    p90FlowTemp;    // 90th percentile flow temp during this cycle (°C)
  float    avgFlowRetDelta;// average (Tboiler - Tret) during this cycle (°C); <0 if no data
  uint8_t  eClass;         // SATCycleClass cast to uint8_t
};

static SATWindowRecord _win4h[SAT_WIN4H_SIZE];
static uint8_t         _win4hHead  = 0;   // next write position
static uint8_t         _win4hCount = 0;   // valid entries (0..SAT_WIN4H_SIZE)

// Accumulator for flow-return delta during active cycle
static float    _cycle_sumFlowRetDelta = 0.0f;
static uint16_t _cycle_deltasamples   = 0;

// --- Per-cycle flow temperature sample buffer (Task #225, p90/p10 classifier) ---
// Samples are collected at the same rate as satCycleSample(); 64 slots covers ~5 min at 5s intervals.
static const uint8_t SAT_FLOW_SAMPLE_SIZE = 64;
static float    _flow_samples[SAT_FLOW_SAMPLE_SIZE];
static uint8_t  _flow_sampleHead  = 0;  // next write position (ring)
static uint8_t  _flow_sampleCount = 0;  // valid sample count (0..SAT_FLOW_SAMPLE_SIZE)

// --- Current Cycle State ---
static bool     _cycle_flameOn          = false;
static uint32_t _cycle_flameOnStartMs   = 0;
static uint32_t _cycle_flameOffStartMs  = 0;
static float    _cycle_maxFlowTemp      = 0.0f;
static float    _cycle_minFlowTemp      = 999.0f;
static float    _cycle_setpointAtStart  = 0.0f;
static float    _cycle_overshootSec     = 0.0f;
static uint32_t _cycle_lastSampleMs     = 0;
static uint16_t _cycle_dhwSamples       = 0;   // samples where DHW was active
static uint16_t _cycle_totalSamples     = 0;   // total samples this cycle
static SATCyclePhase _cycle_phase       = SAT_CP_IDLE;
static uint32_t _cycle_phaseStartMs     = 0;

// --- Sustained State Detection ---
static float    _sustain_overshootSec   = 0.0f;
static float    _sustain_underheatSec   = 0.0f;
static float    _sustain_saturationSec  = 0.0f;
static uint32_t _sustain_lastCheckMs    = 0;
// Initialised to a sentinel far enough in the past that the guard is inactive at boot.
// SAT_DHW_OVERSHOOT_GUARD_MS (300000) is the guard window; subtracting it from 0 wraps to UINT32_MAX-300000,
// which means (millis() - _sustain_dhwEndMs) will always exceed the threshold unless DHW actually ran.
static uint32_t _sustain_dhwEndMs       = (uint32_t)(0UL - SAT_DHW_OVERSHOOT_GUARD_MS); // inactive at boot

// --- EMA Fractions (lightweight sliding window) ---
static float _ema_dutyRatio       = 0.0f;  // fraction of time flame is on
static float _ema_overshootFrac   = 0.0f;  // fraction of cycles with overshoot
static float _ema_underheatFrac   = 0.0f;  // fraction of cycles with underheat
static float _ema_longCycleFrac   = 0.0f;  // fraction of cycles > 600s
static const float EMA_ALPHA      = 0.15f; // decay factor per cycle

// --- Cycle History ---
struct SATCycleRecord {
  SATCycleClass eClass;
  SATCycleKind  eKind;
  float         fDurationSec;
  float         fMaxFlowTemp;
  float         fOvershootSec;
  float         fFlowSetpointError;  // max(flow - setpoint) during cycle
};

static SATCycleRecord _cycleHistory[SAT_CYCLE_HISTORY_SIZE];
static uint8_t        _cycleHistoryIdx = 0;
static uint8_t        _cycleHistoryCount = 0;

//=== Initialize cycle tracking ===
void satCycleInit()
{
  _cycle_flameOn = false;
  _cycle_flameOnStartMs = 0;
  _cycle_flameOffStartMs = millis();
  _cycle_maxFlowTemp = 0.0f;
  _cycle_minFlowTemp = 999.0f;
  _cycle_overshootSec = 0.0f;
  _sustain_overshootSec = 0.0f;
  _sustain_underheatSec = 0.0f;
  _sustain_saturationSec = 0.0f;
  _sustain_lastCheckMs = millis();
  _cycleHistoryIdx = 0;
  _cycleHistoryCount = 0;
  memset(_cycleHistory, 0, sizeof(_cycleHistory));
  // Per-hour counter
  memset(_hourCycleTs, 0, sizeof(_hourCycleTs));
  _hourCycleHead  = 0;
  _hourCycleCount = 0;
  state.sat.iCyclesThisHour = 0;
  // Per-cycle flow sample buffer
  memset(_flow_samples, 0, sizeof(_flow_samples));
  _flow_sampleHead  = 0;
  _flow_sampleCount = 0;
  // Rolling 4-hour window (Task #227)
  memset(_win4h, 0, sizeof(_win4h));
  _win4hHead  = 0;
  _win4hCount = 0;
  _cycle_sumFlowRetDelta = 0.0f;
  _cycle_deltasamples    = 0;
  state.sat.i4hCycles            = 0;
  state.sat.f4hAvgOnSec          = 0.0f;
  state.sat.f4hAvgOffSec         = 0.0f;
  state.sat.f4hAvgFlow           = 0.0f;
  state.sat.f4hDutyRatio         = 0.0f;
  state.sat.f4hOvershootFraction = 0.0f;
  state.sat.f4hUnderheatFraction = 0.0f;
  state.sat.f4hFlowRetDeltaP50   = 0.0f;
  state.sat.f4hFlowRetDeltaP90   = 0.0f;
}

//=== Per-hour cycle counter helpers (Task #203) ===
// Record a flame-on event timestamp in the rolling 60-minute window.
static void _hourCountRecord(uint32_t nowMs)
{
  _hourCycleTs[_hourCycleHead] = nowMs;
  _hourCycleHead = (_hourCycleHead + 1) % SAT_MAX_CYCLES_PER_HOUR;
  if (_hourCycleCount < SAT_MAX_CYCLES_PER_HOUR) _hourCycleCount++;
}

// Count how many recorded timestamps are still within the last 3600s.
// Also evicts stale entries by reducing _hourCycleCount (no explicit removal —
// the ring overwrites them naturally; we just don't count them).
static uint8_t _hourCountGet(uint32_t nowMs)
{
  uint8_t count = 0;
  for (uint8_t i = 0; i < _hourCycleCount; i++) {
    // Walk backwards from (head-1) to find valid entries
    uint8_t idx = (_hourCycleHead + SAT_MAX_CYCLES_PER_HOUR - 1 - i) % SAT_MAX_CYCLES_PER_HOUR;
    if ((nowMs - _hourCycleTs[idx]) < 3600000UL) {
      count++;
    }
  }
  return count;
}

// Public: return true if starting a new PWM ON cycle would exceed the limit.
bool satCycleIsHourLimitReached()
{
  uint32_t nowMs = millis();
  uint8_t count = _hourCountGet(nowMs);
  state.sat.iCyclesThisHour = count;
  return (count >= satGetMaxCyclesPerHour());
}

// Public: current rolling-hour cycle count (for status JSON / MQTT).
uint8_t satCycleGetCyclesThisHour()
{
  uint32_t nowMs = millis();
  uint8_t count = _hourCountGet(nowMs);
  state.sat.iCyclesThisHour = count;
  return count;
}

//=== Rolling 4-hour window statistics (Task #227) ===
// Iterates the ring buffer, filters entries within SAT_WIN4H_SPAN_MS of now,
// and computes: cycle count, avg on/off durations, avg p90 flow temp,
// duty ratio, overshoot/underheat fractions, and flow-return delta p50/p90.
// Results are written directly to state.sat.
void satGetWindow4hStats()
{
  uint32_t nowMs = millis();

  uint32_t sumOnMs      = 0;
  uint32_t sumOffMs     = 0;
  float    sumP90Flow   = 0.0f;
  uint8_t  nOvershoot   = 0;
  uint8_t  nUnderheat   = 0;
  uint8_t  nValid       = 0;

  // Collect per-cycle flow-return deltas into a local scratch array for percentile sort.
  // Max SAT_WIN4H_SIZE (60) floats = 240 bytes on stack — acceptable.
  float deltas[SAT_WIN4H_SIZE];
  uint8_t nDeltas = 0;

  for (uint8_t i = 0; i < _win4hCount; i++) {
    // Walk backwards so newest entries are checked first (avoids counting stale wrap-arounds)
    uint8_t idx = (_win4hHead + SAT_WIN4H_SIZE - 1 - i) % SAT_WIN4H_SIZE;
    if ((nowMs - _win4h[idx].endMs) > SAT_WIN4H_SPAN_MS) continue; // outside 4h window
    sumOnMs    += _win4h[idx].onDurationMs;
    sumOffMs   += _win4h[idx].offDurationMs;
    sumP90Flow += _win4h[idx].p90FlowTemp;
    if (_win4h[idx].eClass == (uint8_t)SAT_CYCLE_OVERSHOOT) nOvershoot++;
    if (_win4h[idx].eClass == (uint8_t)SAT_CYCLE_UNDERHEAT) nUnderheat++;
    if (_win4h[idx].avgFlowRetDelta >= 0.0f) {  // sentinel -1 means no data
      deltas[nDeltas++] = _win4h[idx].avgFlowRetDelta;
    }
    nValid++;
  }

  state.sat.i4hCycles = nValid;

  if (nValid == 0) {
    state.sat.f4hAvgOnSec          = 0.0f;
    state.sat.f4hAvgOffSec         = 0.0f;
    state.sat.f4hAvgFlow           = 0.0f;
    state.sat.f4hDutyRatio         = 0.0f;
    state.sat.f4hOvershootFraction = 0.0f;
    state.sat.f4hUnderheatFraction = 0.0f;
    state.sat.f4hFlowRetDeltaP50   = 0.0f;
    state.sat.f4hFlowRetDeltaP90   = 0.0f;
    return;
  }

  float n = (float)nValid;
  state.sat.f4hAvgOnSec          = (float)sumOnMs  / (n * 1000.0f);
  state.sat.f4hAvgOffSec         = (float)sumOffMs / (n * 1000.0f);
  state.sat.f4hAvgFlow           = sumP90Flow / n;
  state.sat.f4hOvershootFraction = (float)nOvershoot / n;
  state.sat.f4hUnderheatFraction = (float)nUnderheat / n;

  // Duty ratio: on / (on + off) per cycle, averaged
  float totalMs = (float)(sumOnMs + sumOffMs);
  state.sat.f4hDutyRatio = (totalMs > 0.0f) ? ((float)sumOnMs / totalMs) : 0.0f;

  // Percentiles for flow-return delta: insertion-sort the collected deltas
  if (nDeltas == 0) {
    state.sat.f4hFlowRetDeltaP50 = 0.0f;
    state.sat.f4hFlowRetDeltaP90 = 0.0f;
  } else {
    // Insertion sort (n <= 60, runs once per minute — fine)
    for (uint8_t i = 1; i < nDeltas; i++) {
      float key = deltas[i];
      int8_t j = (int8_t)i - 1;
      while (j >= 0 && deltas[j] > key) {
        deltas[j + 1] = deltas[j];
        j--;
      }
      deltas[j + 1] = key;
    }
    uint8_t idx50 = (uint8_t)((uint16_t)50 * (nDeltas - 1) / 100);
    uint8_t idx90 = (uint8_t)((uint16_t)90 * (nDeltas - 1) / 100);
    if (idx50 >= nDeltas) idx50 = nDeltas - 1;
    if (idx90 >= nDeltas) idx90 = nDeltas - 1;
    state.sat.f4hFlowRetDeltaP50 = deltas[idx50];
    state.sat.f4hFlowRetDeltaP90 = deltas[idx90];
  }

  DebugTf(PSTR("SAT 4h: n=%u avgOn=%.0fs avgOff=%.0fs flow=%.1f duty=%.2f overshoot=%.2f underheat=%.2f dP50=%.1f dP90=%.1f\r\n"),
          nValid,
          state.sat.f4hAvgOnSec, state.sat.f4hAvgOffSec,
          state.sat.f4hAvgFlow,
          state.sat.f4hDutyRatio,
          state.sat.f4hOvershootFraction, state.sat.f4hUnderheatFraction,
          state.sat.f4hFlowRetDeltaP50, state.sat.f4hFlowRetDeltaP90);
}

//=== Per-cycle flow temperature p-percentile (Task #225) ===
// Compute approximate percentile from the current flow sample buffer.
// Uses insertion-sort on a local copy (max 64 floats = 256 bytes stack — acceptable).
static float _flowPercentile(uint8_t pct)
{
  if (_flow_sampleCount == 0) return 0.0f;

  // Copy to local buffer for sorting (avoids mutating the ring)
  float sorted[SAT_FLOW_SAMPLE_SIZE];
  uint8_t n = _flow_sampleCount;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t src = (_flow_sampleHead + SAT_FLOW_SAMPLE_SIZE - n + i) % SAT_FLOW_SAMPLE_SIZE;
    sorted[i] = _flow_samples[src];
  }

  // Insertion sort — O(n^2) but n≤64, runs once at cycle end
  for (uint8_t i = 1; i < n; i++) {
    float key = sorted[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }

  // Index for requested percentile (0-based, clamped)
  uint8_t idx = (uint8_t)((uint16_t)pct * (n - 1) / 100);
  if (idx >= n) idx = n - 1;
  return sorted[idx];
}

//=== Determine cycle kind from DHW sample fraction ===
static SATCycleKind _cycleDetectKind()
{
  if (_cycle_totalSamples < 3) return SAT_CK_UNKNOWN;
  float dhwFrac = (float)_cycle_dhwSamples / (float)_cycle_totalSamples;
  if (dhwFrac > 0.8f) return SAT_CK_DHW;
  if (dhwFrac < 0.2f) return SAT_CK_CH;
  return SAT_CK_MIXED;
}

//=== Record a completed cycle into history ===
static void _cycleRecord(SATCycleClass cls, float durationSec, float maxFlow, float overshootSec)
{
  SATCycleKind kind = _cycleDetectKind();
  float flowError = maxFlow - _cycle_setpointAtStart;

  _cycleHistory[_cycleHistoryIdx].eClass = cls;
  _cycleHistory[_cycleHistoryIdx].eKind = kind;
  _cycleHistory[_cycleHistoryIdx].fDurationSec = durationSec;
  _cycleHistory[_cycleHistoryIdx].fMaxFlowTemp = maxFlow;
  _cycleHistory[_cycleHistoryIdx].fOvershootSec = overshootSec;
  _cycleHistory[_cycleHistoryIdx].fFlowSetpointError = flowError;
  _cycleHistoryIdx = (_cycleHistoryIdx + 1) % SAT_CYCLE_HISTORY_SIZE;
  if (_cycleHistoryCount < SAT_CYCLE_HISTORY_SIZE) _cycleHistoryCount++;

  state.sat.iCycleCount++;
  state.sat.eLastCycleClass = cls;
  state.sat.fCycleMaxFlow = maxFlow;
  state.sat.fCycleOvershootSec = overshootSec;
  state.sat.fLastCycleDuration = durationSec;
  state.sat.eLastCycleKind = kind;
  // Compute CH/DHW fractions from sample counts
  if (_cycle_totalSamples > 0) {
    float dhwFrac = (float)_cycle_dhwSamples / (float)_cycle_totalSamples;
    state.sat.fLastCycleFractionDHW = dhwFrac;
    state.sat.fLastCycleFractionCH  = 1.0f - dhwFrac;
  } else {
    state.sat.fLastCycleFractionDHW = 0.0f;
    state.sat.fLastCycleFractionCH  = 0.0f;
  }

  // Update EMA fractions
  _ema_overshootFrac = EMA_ALPHA * (cls == SAT_CYCLE_OVERSHOOT ? 1.0f : 0.0f)
                     + (1.0f - EMA_ALPHA) * _ema_overshootFrac;
  _ema_underheatFrac = EMA_ALPHA * (cls == SAT_CYCLE_UNDERHEAT ? 1.0f : 0.0f)
                     + (1.0f - EMA_ALPHA) * _ema_underheatFrac;
  _ema_longCycleFrac = EMA_ALPHA * (durationSec > 600.0f ? 1.0f : 0.0f)
                     + (1.0f - EMA_ALPHA) * _ema_longCycleFrac;

  // Duty ratio: flame-on time / total time since last cycle
  float totalTime = durationSec;
  if (_cycle_flameOffStartMs > 0 && _cycle_flameOnStartMs > _cycle_flameOffStartMs) {
    totalTime = (float)(millis() - _cycle_flameOffStartMs) / 1000.0f;
  }
  float dutyThis = (totalTime > 0.0f) ? (durationSec / totalTime) : 0.0f;
  if (dutyThis > 1.0f) dutyThis = 1.0f;
  _ema_dutyRatio = EMA_ALPHA * dutyThis + (1.0f - EMA_ALPHA) * _ema_dutyRatio;

  // Expose to state
  state.sat.fDutyRatio = _ema_dutyRatio;
  state.sat.fOvershootFraction = _ema_overshootFrac;
  state.sat.fUnderheatFraction = _ema_underheatFrac;
}

//=== Classify a completed cycle (Task #225: uses p90/p10 flow temp, not max) ===
// p90FlowTemp: 90th percentile of flow temp samples this cycle (robust overshoot indicator)
// p10FlowTemp: 10th percentile of flow temp samples this cycle (robust underheat indicator)
// maxFlowTemp is still recorded for diagnostics but not used for classification.
//
// Verified scenarios (Python CycleClassifier.classify() parity — Task #225 AC#5):
//   Scenario A — GOOD cycle: setpoint=55C, overshootMargin=2C
//     Samples: all between 54C and 56.5C (90+ samples), duration=300s, overshootSec=5s
//     p90=56.2C (<55+2=57 → no overshoot), p10=54.1C (>55-2=53 → no underheat)
//     Classification: GOOD  [would have been OVERSHOOT with max_flow=56.5 if margin=1.5C]
//
//   Scenario B — OVERSHOOT cycle: setpoint=55C, overshootMargin=2C
//     Samples: 90% between 57C and 58C, 10% spike to 62C at ignition, duration=400s, overshootSec=120s
//     p90=57.8C (>57 → overshoot) AND overshootSec=120>10: OVERSHOOT
//     With old max_flow classifier: also OVERSHOOT. No regression.
//
//   Scenario C — UNDERHEAT cycle: setpoint=55C, overshootMargin=2C
//     Samples: all between 48C and 52C, duration=250s, overshootSec=0s
//     p90=51.8C (<57 → no overshoot), p10=48.2C (<53 → underheat): UNDERHEAT
//     With old classifier: also UNDERHEAT (min_flow=48.2<53). No regression.
//     [p10 advantage: brief cold pocket at ignition start doesn't skew result]
static SATCycleClass _cycleClassify(float durationSec, float p90FlowTemp, float p10FlowTemp, float setpoint, float overshootSec)
{
  // Too short → short cycling (unchanged per AC#4)
  if (durationSec < SAT_CYCLE_SHORT_DURATION_SEC) {
    return SAT_CYCLE_SHORT;
  }

  // Significant overshoot: p90 of flow temp exceeds setpoint + margin AND overshoot timer confirms.
  // Using p90 rather than max_flow avoids false OVERSHOOT from brief ignition spikes.
  // Equivalent to Python CycleClassifier.classify() tail-percentile check.
  if (p90FlowTemp > setpoint + settings.sat.fOvershootMargin && overshootSec > 10.0f) {
    return SAT_CYCLE_OVERSHOOT;
  }

  // Flow never reached setpoint → underheat.
  // Using p10 rather than min_flow gives a stable signal not skewed by brief cold pockets
  // (e.g. sensor lag at flame-on). Mirrors Python use of lower-percentile flow check.
  if (p10FlowTemp < setpoint - SAT_UNDERSHOOT_MARGIN_C) {
    return SAT_CYCLE_UNDERHEAT;
  }

  // Not enough data for a confident classification
  if (durationSec < SAT_CYCLE_SHORT_DURATION_SEC * 2) {
    return SAT_CYCLE_UNCERTAIN;
  }

  return SAT_CYCLE_GOOD;
}

//=== Called on each flame status change ===
void satCycleOnFlameChange(bool flameOn)
{
  uint32_t now = millis();

  if (flameOn && !_cycle_flameOn) {
    // Flame just turned ON — start new cycle
    _cycle_flameOn = true;
    _cycle_flameOnStartMs = now;
    _cycle_maxFlowTemp = OTcurrentSystemState.Tboiler;
    _cycle_minFlowTemp = OTcurrentSystemState.Tboiler;
    _cycle_setpointAtStart = state.sat.fFinalSetpoint;
    _cycle_overshootSec = 0.0f;
    _cycle_lastSampleMs = now;
    _cycle_dhwSamples = 0;
    _cycle_totalSamples = 0;
    // Record flame-on event in the rolling per-hour counter (Task #203)
    _hourCountRecord(now);
    state.sat.iCyclesThisHour = _hourCountGet(now);
    // Reset per-cycle flow sample buffer for p90/p10 classification (Task #225)
    _flow_sampleHead  = 0;
    _flow_sampleCount = 0;
    // Seed with current boiler temp so we have at least one sample
    _flow_samples[_flow_sampleHead] = OTcurrentSystemState.Tboiler;
    _flow_sampleHead = (_flow_sampleHead + 1) % SAT_FLOW_SAMPLE_SIZE;
    _flow_sampleCount = 1;
    // Reset flow-return delta accumulator (Task #227)
    _cycle_sumFlowRetDelta = 0.0f;
    _cycle_deltasamples    = 0;
  }
  else if (!flameOn && _cycle_flameOn) {
    // Flame just turned OFF — complete the cycle
    _cycle_flameOn = false;
    _cycle_flameOffStartMs = now;
    _cycle_phase = SAT_CP_COOLDOWN;
    _cycle_phaseStartMs = now;

    float durationSec = (float)(now - _cycle_flameOnStartMs) / 1000.0f;
    // Compute p90/p10 from collected flow samples (Task #225)
    float p90 = (_flow_sampleCount >= 10) ? _flowPercentile(90) : _cycle_maxFlowTemp;
    float p10 = (_flow_sampleCount >= 10) ? _flowPercentile(10) : _cycle_minFlowTemp;
    SATCycleClass cls = _cycleClassify(durationSec, p90, p10,
                                        _cycle_setpointAtStart, _cycle_overshootSec);
    _cycleRecord(cls, durationSec, _cycle_maxFlowTemp, _cycle_overshootSec);

    // Record completed cycle into the rolling 4-hour window (Task #227)
    {
      uint32_t onMs  = now - _cycle_flameOnStartMs;
      uint32_t offMs = (_cycle_flameOffStartMs > 0 && _cycle_flameOnStartMs > _cycle_flameOffStartMs)
                       ? (_cycle_flameOnStartMs - _cycle_flameOffStartMs)
                       : 0;
      float avgDelta = (_cycle_deltasamples > 0)
                       ? (_cycle_sumFlowRetDelta / (float)_cycle_deltasamples)
                       : -1.0f;  // sentinel: no valid data
      _win4h[_win4hHead].endMs          = now;
      _win4h[_win4hHead].onDurationMs   = onMs;
      _win4h[_win4hHead].offDurationMs  = offMs;
      _win4h[_win4hHead].p90FlowTemp    = p90;
      _win4h[_win4hHead].avgFlowRetDelta = avgDelta;
      _win4h[_win4hHead].eClass         = (uint8_t)cls;
      _win4hHead = (_win4hHead + 1) % SAT_WIN4H_SIZE;
      if (_win4hCount < SAT_WIN4H_SIZE) _win4hCount++;
    }

    DebugTf(PSTR("SAT cycle #%d: class=%d dur=%.0fs maxFlow=%.1f p90=%.1f p10=%.1f overshoot=%.0fs\r\n"),
            state.sat.iCycleCount, (int)cls, durationSec,
            _cycle_maxFlowTemp, p90, p10, _cycle_overshootSec);
  }
}

//=== Called periodically to sample flow temp during active cycle ===
void satCycleSample()
{
  if (!_cycle_flameOn) return;

  uint32_t now = millis();
  float flowTemp = OTcurrentSystemState.Tboiler;

  if (flowTemp > _cycle_maxFlowTemp) _cycle_maxFlowTemp = flowTemp;
  if (flowTemp < _cycle_minFlowTemp) _cycle_minFlowTemp = flowTemp;

  // Accumulate flow-return delta for 4-hour window record (Task #227)
  // Tret may be 0 when return temp sensor is absent; only accumulate when plausible.
  float retTemp = OTcurrentSystemState.Tret;
  if (retTemp > 10.0f && retTemp < 100.0f) {
    _cycle_sumFlowRetDelta += (flowTemp - retTemp);
    _cycle_deltasamples++;
  }

  // Collect flow temperature sample for p90/p10 classifier (Task #225)
  _flow_samples[_flow_sampleHead] = flowTemp;
  _flow_sampleHead = (_flow_sampleHead + 1) % SAT_FLOW_SAMPLE_SIZE;
  if (_flow_sampleCount < SAT_FLOW_SAMPLE_SIZE) _flow_sampleCount++;

  // Track overshoot seconds: time flow temp is above setpoint + margin
  float elapsed = (float)(now - _cycle_lastSampleMs) / 1000.0f;
  if (flowTemp > _cycle_setpointAtStart + settings.sat.fOvershootMargin) {
    _cycle_overshootSec += elapsed;
  }

  // Track DHW vs CH samples for cycle kind detection
  _cycle_totalSamples++;
  if ((OTcurrentSystemState.Statusflags & 0x04) != 0) {  // Bit 2 = DHW active
    _cycle_dhwSamples++;
  }

  // Phase detection (Task #35)
  SATCyclePhase newPhase = _cycle_phase;
  float setpoint = _cycle_setpointAtStart;
  float band = 1.5f;  // at-setpoint band

  if (flowTemp < setpoint - band) {
    newPhase = SAT_CP_STARTUP;
  } else if (fabsf(flowTemp - setpoint) <= band) {
    newPhase = SAT_CP_STEADY;
  }
  // Cooldown is set on flame-off in satCycleOnFlameChange

  if (newPhase != _cycle_phase) {
    _cycle_phase = newPhase;
    _cycle_phaseStartMs = now;
  }

  _cycle_lastSampleMs = now;
}

//=== Check if auto-switch between PWM and continuous is needed ===
// Called from the control loop. Returns true if mode was switched.
bool satCycleCheckAutoSwitch()
{
  if (!settings.sat.bPwmAutoSwitch) return false;

  uint32_t now = millis();
  float dt = (float)(now - _sustain_lastCheckMs) / 1000.0f;
  _sustain_lastCheckMs = now;
  if (dt <= 0.0f || dt > 60.0f) return false; // Skip on first call or large gaps

  float flowTemp = OTcurrentSystemState.Tboiler;
  float setpoint = state.sat.fFinalSetpoint;

  // --- DHW post-overshoot guard: track end of DHW heating ---
  if (state.sat.bDhwActive) {
    _sustain_dhwEndMs = now; // Keep refreshing while DHW is active
  }

  // --- Overshoot detection (continuous → PWM) ---
  if (state.sat.eControlMode == SAT_MODE_CONTINUOUS) {
    // Skip overshoot→PWM switch while DHW is active or within 300s of DHW end
    bool dhwGuardActive = state.sat.bDhwActive ||
                          (now - _sustain_dhwEndMs < SAT_DHW_OVERSHOOT_GUARD_MS);
    if (!dhwGuardActive && flowTemp > setpoint + settings.sat.fOvershootMargin) {
      _sustain_overshootSec += dt;
    } else {
      _sustain_overshootSec = 0.0f;
    }
    if (_sustain_overshootSec >= SAT_OVERSHOOT_SUSTAIN_SEC) {
      DebugTln(F("SAT: sustained overshoot detected, switching to PWM mode"));
      state.sat.eControlMode = SAT_MODE_PWM;
      _sustain_overshootSec = 0.0f;
      return true;
    }
  }

  // --- Underheat detection (PWM → continuous) ---
  if (state.sat.eControlMode == SAT_MODE_PWM) {
    if (flowTemp < setpoint - SAT_UNDERSHOOT_MARGIN_C) {
      _sustain_underheatSec += dt;
    } else {
      _sustain_underheatSec = 0.0f;
    }
    if (_sustain_underheatSec >= SAT_UNDERHEAT_SUSTAIN_SEC) {
      DebugTln(F("SAT: sustained underheat detected, switching to continuous mode"));
      state.sat.eControlMode = SAT_MODE_CONTINUOUS;
      _sustain_underheatSec = 0.0f;
      return true;
    }

    // --- Saturation detection (PWM → continuous): off-time stays 0 too long ---
    if (_cycle_flameOn) {
      float flameDur = (float)(now - _cycle_flameOnStartMs) / 1000.0f;
      if (flameDur > SAT_SATURATION_SUSTAIN_SEC) {
        DebugTln(F("SAT: PWM saturation detected, switching to continuous mode"));
        state.sat.eControlMode = SAT_MODE_CONTINUOUS;
        _sustain_saturationSec = 0.0f;
        return true;
      }
    }
  }

  return false;
}

//=== Get cycle history count of a specific class (for diagnostics) ===
uint8_t satCycleCountClass(SATCycleClass cls)
{
  uint8_t count = 0;
  for (uint8_t i = 0; i < _cycleHistoryCount; i++) {
    if (_cycleHistory[i].eClass == cls) count++;
  }
  return count;
}

//=== Accessors for cycle timing (used by SATcontrol.ino PWM mode) ===
uint32_t satCycleGetFlameOnStartMs()  { return _cycle_flameOnStartMs; }
uint32_t satCycleGetFlameOffStartMs() { return _cycle_flameOffStartMs; }

//=== Cycle phase name (Task #35) ===
const char* satCycleGetPhaseName()
{
  switch (_cycle_phase) {
    case SAT_CP_STARTUP:  return "startup";
    case SAT_CP_STEADY:   return "steady";
    case SAT_CP_COOLDOWN: return "cooldown";
    default:              return "idle";
  }
}

uint32_t satCycleGetPhaseDurationSec()
{
  if (_cycle_phaseStartMs == 0) return 0;
  return (millis() - _cycle_phaseStartMs) / 1000;
}

