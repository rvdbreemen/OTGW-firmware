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
**  TERMS OF USE: GNU GPLv3. See bottom of OTGW-firmware.h
***************************************************************************
*/

// Per-module conditional debug — toggle with key '5' in telnet debug menu
#define SATDebugTf(fmt, ...)  do { if (state.debug.bSAT) DebugTf(fmt,  ##__VA_ARGS__); } while(0)
#define SATDebugTln(s)        do { if (state.debug.bSAT) DebugTln(s);                  } while(0)
#define SATDebugf(fmt, ...)   do { if (state.debug.bSAT) Debugf(fmt,   ##__VA_ARGS__); } while(0)

// --- Cycle Kind & Phase enums are in OTGW-firmware.h ---

// --- Cycle Constants ---
static const uint8_t  SAT_CYCLE_HISTORY_SIZE       = 16;
// Overshoot margin now read from settings.sat.fOvershootMargin (default 2.0C; Python OVERSHOOT_MARGIN_CELSIUS is 3.0)
static const float    SAT_OVERSHOOT_SUSTAIN_SEC     = 60.0f;   // Sustained overshoot before PWM switch
static const float    SAT_UNDERSHOOT_MARGIN_C       = 3.0f;    // Below-setpoint margin for underheat (Python UNDERSHOOT_MARGIN_CELSIUS=-3.0)
static const float    SAT_UNDERHEAT_SUSTAIN_SEC     = 180.0f;  // Sustained underheat before continuous switch
static const float    SAT_SATURATION_SUSTAIN_SEC    = 300.0f;  // Sustained saturation before continuous switch
static const uint32_t SAT_DHW_OVERSHOOT_GUARD_MS    = 300000UL; // 300s guard: skip overshoot->PWM switch during/after DHW

// --- Per-hour PWM start counter (rolling 60-minute window, Task #203, TASK-1401) ---
// Holds PWM's own ON-phase starts only (thermo-nova pwm.py:159-160; Sergeant D entry 59): hot-water
// ignitions and relights are not recorded. Max cycles/hour is capped at 6 by settings; the ring holds 6.
static const uint8_t SAT_MAX_CYCLES_PER_HOUR = 6;
static uint32_t _hourCycleTs[SAT_MAX_CYCLES_PER_HOUR]; // millis() of each PWM ON-phase start
static uint8_t  _hourCycleHead  = 0;  // next write position
static uint8_t  _hourCycleCount = 0;  // valid entries (0..SAT_MAX_CYCLES_PER_HOUR)

// --- Rolling 4-hour cycle window (Task #227) ---
// SAT_WIN4H_SIZE and SATWindowRecord are defined ahead of all .ino files
// (the size in boards.h, the record struct in SATtypes.h).
static const uint32_t SAT_WIN4H_SPAN_MS = 4UL * 3600UL * 1000UL; // 4 hours in ms

static SATWindowRecord _win4h[SAT_WIN4H_SIZE];
static SAT_RING_IDX_T  _win4hHead  = 0;   // next write position
static SAT_RING_IDX_T  _win4hCount = 0;   // valid entries (0..SAT_WIN4H_SIZE)

// --- Rolling 24-hour DAILY window (TASK-891.4, Python DAILY_WINDOW_SECONDS) ---
// HEAP DISCIPLINE (AC#6): a full per-cycle 24h ring at 4h-window density would be
// ~6x the 4h ring (~52 KB on ESP32) and blow the 40k heap floor. Instead the daily
// window is a REDUCED-RESOLUTION aggregate: 24 coarse hourly buckets holding running
// counts/sums only. Duty and the overshoot/underheat/long-cycle fractions are exact
// from these sums; per-cycle percentiles and the true median-on-duration are DROPPED
// (documented reduction — the 4h window still carries full-depth percentiles).
// Cost: SAT_DAILY_BUCKETS * sizeof(SATDailyBucket) = 24 * 20 = 480 bytes static DRAM.
static const uint32_t SAT_DAILY_BUCKET_MS = 3600UL * 1000UL; // 1-hour bucket granularity
static const uint8_t  SAT_DAILY_BUCKETS   = 24;              // 24 buckets => 24h rolling window
static const float    SAT_LONG_CYCLE_SEC  = 600.0f;         // >= this on-time counts as a "long" cycle (Python TARGET_MIN_ON_TIME_SECONDS)

struct SATDailyBucket {
  uint32_t bucketIdx;    // millis()/SAT_DAILY_BUCKET_MS this bucket represents (0 = empty slot)
  uint16_t nCycles;      // cycles ending in this bucket-hour
  uint16_t nOvershoot;   // of which classified OVERSHOOT
  uint16_t nUnderheat;   // of which UNDERHEAT or UNDERHEAT_PWM
  uint16_t nLong;        // of which on-time >= SAT_LONG_CYCLE_SEC
  uint32_t sumOnMs;      // total flame-on ms
  uint32_t sumOffMs;     // total flame-off ms (for duty)
};
static SATDailyBucket _daily[SAT_DAILY_BUCKETS];
static uint8_t        _dailyHead  = 0;   // next write position
static uint8_t        _dailyCount = 0;   // valid buckets (0..SAT_DAILY_BUCKETS)

// Forward decl: satGetColdSetpoint() is static in SATcontrol.ino (compiled before
// this file in the single-TU concatenation). Declared here so a linkage surprise
// surfaces at compile time rather than silently.
static float satGetColdSetpoint();

// TASK-891.4 classifier / shape-metric constants
static const float SAT_IN_BAND_MARGIN_C     = 1.0f;   // |flow - control_sp| <= this => in-band (Python IN_BAND_MARGIN_CELSIUS)
static const float SAT_SHAPE_WARMUP_SEC     = 60.0f;  // shape metrics ignore the first N s of a cycle (Python effective_warmup cap = min(60, 0.25*duration); streaming approx uses the 60s cap)

// Accumulator for flow-return delta during active cycle
static float    _cycle_sumFlowRetDelta = 0.0f;
static uint16_t _cycle_deltasamples   = 0;

// --- Per-cycle flow temperature sample buffer (Task #225, p90/p10 classifier) ---
// SAT_FLOW_SAMPLE_SIZE is board-defined in boards.h (64 on ESP8266, 256 on ESP32).
static float    _flow_samples[SAT_FLOW_SAMPLE_SIZE];
static SAT_RING_IDX_T _flow_sampleHead  = 0;  // next write position (ring)
static SAT_RING_IDX_T _flow_sampleCount = 0;  // valid sample count (0..SAT_FLOW_SAMPLE_SIZE)
static uint8_t  _flow_logTick = 0;            // rate limit of the 'SAT sample' debug line

// --- Tail ring buffer for end-of-cycle 180s classification window ---
// Sampled at 1Hz for a true time-based window, independent of loop rate.
// SAT_TAIL_SAMPLE_SIZE is board-defined in boards.h (64 on ESP8266, 180 on ESP32).
static float    _tail_samples[SAT_TAIL_SAMPLE_SIZE];
static uint8_t  _tail_sampleHead  = 0;
static uint8_t  _tail_sampleCount = 0;
static uint32_t _tail_lastSampleMs = 0;

// --- Current Cycle State ---
static bool     _cycle_flameOn          = false;
static uint32_t _cycle_flameOnStartMs   = 0;
static uint32_t _cycle_flameOffStartMs  = 0;
static float    _cycle_maxFlowTemp      = 0.0f;
static float    _cycle_minFlowTemp      = 999.0f;
static float    _cycle_setpointAtStart  = 0.0f;
static float    _cycle_overshootSec     = 0.0f;
static uint32_t _cycle_lastSampleMs     = 0;
// Cycle kind and hot-water-in-tail inputs, counted at 1 Hz in satCycleSample() (TASK-1387).
// uint32_t: a cycle of more than 65535 s (18 h of continuous burn) must not wrap the counters.
static uint32_t _cycle_dhwSamples       = 0;   // samples with DHW active (Statusflags bit 2), tracker.py:103
static uint32_t _cycle_chSamples        = 0;   // samples with CH enabled and no DHW, tracker.py:104
static uint32_t _cycle_totalSamples     = 0;   // total 1 Hz samples this cycle (tracker.py sample_count)
static uint32_t _cycle_lastDhwMs        = 0;   // time of the last DHW sample, ms after flame-on (DHW-in-tail test; valid while _cycle_dhwSamples > 0)
static SATCyclePhase _cycle_phase       = SAT_CP_IDLE;
static uint32_t _cycle_phaseStartMs     = 0;

// --- TASK-891.4: per-cycle shape metrics (AC#4) + off-with-demand (AC#5) ---
// Accumulated in satCycleSample() (post-warmup), snapshotted to state.sat at flame-off.
static float    _cycle_timeInBandSec    = 0.0f;   // sum of intervals with |flow - control_sp| <= SAT_IN_BAND_MARGIN_C
static float    _cycle_totalOvershootSec = 0.0f;  // sum of intervals with (flow - control_sp) >= overshoot margin
static float    _cycle_overshootStreakSec = 0.0f; // current contiguous overshoot run length
static float    _cycle_timeToFirstOvershootSec = -1.0f;     // -1 = none yet
static float    _cycle_timeToSustainedOvershootSec = -1.0f; // -1 = none yet
static uint32_t _cycle_lastFlameOffMs   = 0;      // millis() of the previous flame-off edge (for off-with-demand gating)

// --- Sustained State Detection ---
// Sustained overshoot (continuous -> PWM): the millis() of the first qualifying sample, counted as thermo-nova
// sustained_runtime() does (helpers.py:51-56; Sergeant D entry 41). Valid only while _sustain_overshootRunning.
static bool     _sustain_overshootRunning = false;
static uint32_t _sustain_overshootStartMs = 0;
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

// thermo-nova _reset_pwm_disable_guards() (heating_control.py:525-528): clear the sustained overshoot,
// underheat and saturation timers.
void satCycleResetSustainGuards()
{
  _sustain_overshootRunning = false;
  _sustain_underheatSec = 0.0f;
  _sustain_saturationSec = 0.0f;
}

// A control tick that does not reach satCycleCheckAutoSwitch() is a tick without a qualifying sample, and
// clears the overshoot start so the next overshoot counts its 60 s from scratch. thermo-nova does the same
// for hot water (heating_control.py:268-270), the DHW guard (:272-274), heater off or a requested setpoint
// at or below the cold setpoint (:276-282) and PWM enabled (:264-266). The other callers are firmware
// choices without a thermo-nova counterpart: boiler fault, thermal-safe fallback, summer simmer, and
// SAT off / OT loss / re-enable (thermo-nova keeps the start there: climate.py:544-545 returns before update()).
void satCycleSkipAutoSwitch()
{
  _sustain_overshootRunning = false;
}

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

// --- Heating Curve Recommendation (HCR) — buffer declares (Task #228) ---
// Defined here so satCycleInit() can zero them before _hcrIntraMedian() is defined below.

// HCR_DAYS (daily-median ring) and HCR_INTRADAY_SIZE (intra-day sample ring) are
// board-defined in boards.h (ESP-abstraction Tier 3): 7/96 on ESP8266, 30/1440
// on ESP32.
static const float    HCR_THRESHOLD_C   = 0.5f;  // median error threshold (°C)
static const uint8_t  HCR_SUSTAIN_DAYS  = 3;     // consecutive days needed for a recommendation
static const char SAT_HCR_FILE_OLD[] PROGMEM = "/sat_hcr.json";
static const char SAT_HCR_FILE[]     PROGMEM = "/sat/sat_hcr.json";

// Ring buffer of daily median errors (oldest → newest)
static float    _hcr_dailyMedian[HCR_DAYS]; // raw daily medians
static uint8_t  _hcr_head   = 0;            // next write position (HCR_DAYS <= 30, fits uint8)
static uint8_t  _hcr_count  = 0;            // valid entries (0..HCR_DAYS)

// Intra-day sample accumulator: collect (room - target) readings until midnight.
// ESP8266: 96 samples at 15-min intervals = one day. ESP32: 1440 samples at 1-min intervals.
static float    _hcr_samples[HCR_INTRADAY_SIZE];
static SAT_RING_IDX_T _hcr_sHead    = 0;    // next write position
static SAT_RING_IDX_T _hcr_sCount   = 0;    // valid samples
static uint32_t _hcr_lastDayNum = 0;        // last calendar day that was committed (time/86400)

//--- Forward declarations for HCR functions ---
static float _hcrIntraMedian();
void satHCRSaveState();
void satHCRLoadState();

//=== Initialize cycle tracking ===
void satCycleInit()
{
  _cycle_flameOn = false;
  _cycle_flameOnStartMs = 0;
  _cycle_flameOffStartMs = millis();
  _cycle_maxFlowTemp = 0.0f;
  _cycle_minFlowTemp = 999.0f;
  _cycle_overshootSec = 0.0f;
  satCycleResetSustainGuards();
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
  // Tail ring buffer (180s window)
  memset(_tail_samples, 0, sizeof(_tail_samples));
  _tail_sampleHead  = 0;
  _tail_sampleCount = 0;
  _tail_lastSampleMs = 0;
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
  // Rolling 24-hour DAILY window (TASK-891.4)
  memset(_daily, 0, sizeof(_daily));
  _dailyHead  = 0;
  _dailyCount = 0;
  state.sat.i24hCycles            = 0;
  state.sat.f24hDutyRatio         = 0.0f;
  state.sat.f24hOvershootFraction = 0.0f;
  state.sat.f24hUnderheatFraction = 0.0f;
  state.sat.f24hLongCycleFraction = 0.0f;
  // Per-cycle shape metrics + off-with-demand (TASK-891.4)
  _cycle_timeInBandSec               = 0.0f;
  _cycle_totalOvershootSec           = 0.0f;
  _cycle_overshootStreakSec          = 0.0f;
  _cycle_timeToFirstOvershootSec     = -1.0f;
  _cycle_timeToSustainedOvershootSec = -1.0f;
  _cycle_lastFlameOffMs              = millis();
  state.sat.fCycleReqSetpointError        = 0.0f;
  state.sat.fCycleTimeInBandSec           = 0.0f;
  state.sat.fCycleTotalOvershootSec       = 0.0f;
  state.sat.fCycleTimeToFirstOvershoot    = -1.0f;
  state.sat.fCycleTimeToSustainedOvershoot = -1.0f;
  state.sat.fOffWithDemandSec             = 0.0f;
  // Daily median recommendation (Task #228): reset intra-day buffer, keep daily ring intact
  _hcr_sHead  = 0;
  _hcr_sCount = 0;
  strlcpy(state.sat.sHeatCurveRec, "insufficient", sizeof(state.sat.sHeatCurveRec));
}

//=== Per-hour PWM start counter helpers (Task #203, TASK-1401) ===
// Record a PWM ON-phase start timestamp in the rolling 60-minute window.
static void _hourCountRecord(uint32_t nowMs)
{
  _hourCycleTs[_hourCycleHead] = nowMs;
  _hourCycleHead = (_hourCycleHead + 1) % SAT_MAX_CYCLES_PER_HOUR;
  if (_hourCycleCount < SAT_MAX_CYCLES_PER_HOUR) _hourCycleCount++;
}

// Count how many recorded timestamps are still within the last 3600s.
// Walks newest to oldest (the entries are in time order) and evicts the first stale entry and all older
// ones by lowering _hourCycleCount, so after a millis() wrap an old timestamp cannot look recent again.
static uint8_t _hourCountGet(uint32_t nowMs)
{
  for (uint8_t i = 0; i < _hourCycleCount; i++) {
    uint8_t idx = (_hourCycleHead + SAT_MAX_CYCLES_PER_HOUR - 1 - i) % SAT_MAX_CYCLES_PER_HOUR;
    if ((nowMs - _hourCycleTs[idx]) >= 3600000UL) {
      _hourCycleCount = i;
      break;
    }
  }
  return _hourCycleCount;
}

// Public: record a PWM ON-phase start, once per ON phase (OFF->ON, duty range 5, the ON phase at PWM
// entry), at the start, lit or not (satPwmBeginOn() in SATcontrol.ino); a flame edge never adds a count. The only writer
// of the ring (Sergeant D entries 59, 90, 95, pwm.py:159-160).
void satCycleRecordPwmStart()
{
  uint32_t nowMs = millis();
  _hourCountRecord(nowMs);
  state.sat.iCyclesThisHour = _hourCountGet(nowMs);
}

// Public: return true if starting a new PWM ON cycle would exceed the limit. A heat pump has no hourly
// start limit (Sergeant D entry 81): its starts are still recorded and reported, never refused.
bool satCycleIsHourLimitReached()
{
  uint32_t nowMs = millis();
  uint8_t count = _hourCountGet(nowMs);
  state.sat.iCyclesThisHour = count;
  if (satGetEffectiveHeatingSource() == SAT_SRC_HEAT_PUMP) return false;
  return (count >= satGetMaxCyclesPerHour());
}

// Public: PWM ON-phase starts in the rolling hour (status JSON / MQTT cycles_this_hour).
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
  uint16_t nOvershoot   = 0;
  uint16_t nUnderheat   = 0;
  uint16_t nValid       = 0;

  // Collect per-cycle flow-return deltas into a scratch array for percentile sort.
  // static: avoids 1440 bytes on the ESP32 stack (SAT_WIN4H_SIZE=360 on ESP32).
  // Safe: satUpdate4hWindow() is only ever called from the main loop, never re-entrant.
  static float deltas[SAT_WIN4H_SIZE];
  uint16_t nDeltas = 0;

  for (uint16_t i = 0; i < _win4hCount; i++) {
    // Walk backwards so newest entries are checked first (avoids counting stale wrap-arounds)
    uint16_t idx = (_win4hHead + SAT_WIN4H_SIZE - 1 - i) % SAT_WIN4H_SIZE;
    if ((nowMs - _win4h[idx].endMs) > SAT_WIN4H_SPAN_MS) continue; // outside 4h window
    sumOnMs    += _win4h[idx].onDurationMs;
    sumOffMs   += _win4h[idx].offDurationMs;
    sumP90Flow += _win4h[idx].p90FlowTemp;
    if (_win4h[idx].eClass == (uint8_t)SAT_CYCLE_OVERSHOOT) nOvershoot++;
    // Both continuous and PWM underheat count toward the underheat fraction (TASK-891.4).
    if (_win4h[idx].eClass == (uint8_t)SAT_CYCLE_UNDERHEAT ||
        _win4h[idx].eClass == (uint8_t)SAT_CYCLE_UNDERHEAT_PWM) nUnderheat++;
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
    // Insertion sort (n <= SAT_WIN4H_SIZE, runs once per minute)
    for (uint16_t i = 1; i < nDeltas; i++) {
      float key = deltas[i];
      int16_t j = (int16_t)i - 1;
      while (j >= 0 && deltas[j] > key) {
        deltas[j + 1] = deltas[j];
        j--;
      }
      deltas[j + 1] = key;
    }
    uint16_t idx50 = (uint16_t)((uint32_t)50 * (nDeltas - 1) / 100);
    uint16_t idx90 = (uint16_t)((uint32_t)90 * (nDeltas - 1) / 100);
    if (idx50 >= nDeltas) idx50 = nDeltas - 1;
    if (idx90 >= nDeltas) idx90 = nDeltas - 1;
    state.sat.f4hFlowRetDeltaP50 = deltas[idx50];
    state.sat.f4hFlowRetDeltaP90 = deltas[idx90];
  }

  SATDebugTf(PSTR("SAT 4h: n=%u avgOn=%.0fs avgOff=%.0fs flow=%.1f duty=%.2f overshoot=%.2f underheat=%.2f dP50=%.1f dP90=%.1f\r\n"),
          nValid,
          state.sat.f4hAvgOnSec, state.sat.f4hAvgOffSec,
          state.sat.f4hAvgFlow,
          state.sat.f4hDutyRatio,
          state.sat.f4hOvershootFraction, state.sat.f4hUnderheatFraction,
          state.sat.f4hFlowRetDeltaP50, state.sat.f4hFlowRetDeltaP90);
}

//=== Rolling 24-hour DAILY window (TASK-891.4) ===
// Record a completed cycle into the coarse hourly-bucket ring. Reuses the current
// bucket while the cycle-end falls in the same wall-clock hour, else opens a new one.
static void _dailyRecord(uint32_t nowMs, uint32_t onMs, uint32_t offMs,
                         SATCycleClass cls, float durationSec)
{
  uint32_t bidx = nowMs / SAT_DAILY_BUCKET_MS;

  // Locate the most-recently-written bucket; reuse it if it is the same hour.
  uint8_t slot;
  if (_dailyCount > 0) {
    uint8_t last = (uint8_t)((_dailyHead + SAT_DAILY_BUCKETS - 1) % SAT_DAILY_BUCKETS);
    if (_daily[last].bucketIdx == bidx) {
      slot = last;   // same hour: accumulate into existing bucket
    } else {
      slot = _dailyHead;   // new hour: open a fresh bucket
      memset(&_daily[slot], 0, sizeof(_daily[slot]));
      _daily[slot].bucketIdx = bidx;
      _dailyHead = (uint8_t)((_dailyHead + 1) % SAT_DAILY_BUCKETS);
      if (_dailyCount < SAT_DAILY_BUCKETS) _dailyCount++;
    }
  } else {
    slot = _dailyHead;
    memset(&_daily[slot], 0, sizeof(_daily[slot]));
    _daily[slot].bucketIdx = bidx;
    _dailyHead = (uint8_t)((_dailyHead + 1) % SAT_DAILY_BUCKETS);
    _dailyCount = 1;
  }

  _daily[slot].nCycles++;
  if (cls == SAT_CYCLE_OVERSHOOT) _daily[slot].nOvershoot++;
  if (cls == SAT_CYCLE_UNDERHEAT || cls == SAT_CYCLE_UNDERHEAT_PWM) _daily[slot].nUnderheat++;
  if (durationSec >= SAT_LONG_CYCLE_SEC) _daily[slot].nLong++;
  _daily[slot].sumOnMs  += onMs;
  _daily[slot].sumOffMs += offMs;
}

// Aggregate the last 24h of hourly buckets into state.sat.f24h*. Reduced-resolution:
// counts/sums only (duty + fractions are exact; percentiles/median are not carried).
void satGetWindow24hStats()
{
  uint32_t bidx   = millis() / SAT_DAILY_BUCKET_MS;
  uint32_t cutoff = (bidx >= (uint32_t)SAT_DAILY_BUCKETS) ? (bidx - SAT_DAILY_BUCKETS + 1) : 0;

  uint32_t nC = 0, nO = 0, nU = 0, nL = 0;
  uint64_t sOn = 0, sOff = 0;
  for (uint8_t i = 0; i < _dailyCount; i++) {
    if (_daily[i].nCycles == 0) continue;
    if (_daily[i].bucketIdx < cutoff || _daily[i].bucketIdx > bidx) continue; // outside 24h
    nC  += _daily[i].nCycles;
    nO  += _daily[i].nOvershoot;
    nU  += _daily[i].nUnderheat;
    nL  += _daily[i].nLong;
    sOn += _daily[i].sumOnMs;
    sOff += _daily[i].sumOffMs;
  }

  state.sat.i24hCycles = (uint16_t)((nC > 0xFFFFu) ? 0xFFFFu : nC);
  if (nC == 0) {
    state.sat.f24hDutyRatio         = 0.0f;
    state.sat.f24hOvershootFraction = 0.0f;
    state.sat.f24hUnderheatFraction = 0.0f;
    state.sat.f24hLongCycleFraction = 0.0f;
    return;
  }
  float fn = (float)nC;
  float totalMs = (float)(sOn + sOff);
  state.sat.f24hDutyRatio         = (totalMs > 0.0f) ? ((float)sOn / totalMs) : 0.0f;
  state.sat.f24hOvershootFraction = (float)nO / fn;
  state.sat.f24hUnderheatFraction = (float)nU / fn;
  state.sat.f24hLongCycleFraction = (float)nL / fn;

  SATDebugTf(PSTR("SAT 24h: n=%lu duty=%.2f overshoot=%.2f underheat=%.2f long=%.2f\r\n"),
          (unsigned long)nC, state.sat.f24hDutyRatio,
          state.sat.f24hOvershootFraction, state.sat.f24hUnderheatFraction,
          state.sat.f24hLongCycleFraction);
}

//=== Per-cycle flow temperature p-percentile (Task #225) ===
// Compute approximate percentile from the current flow sample buffer: 1 Hz samples, so
// the last SAT_FLOW_SAMPLE_SIZE seconds of the cycle (TASK-1269).
// Uses insertion-sort on a local copy (SAT_FLOW_SAMPLE_SIZE floats on the stack).
static float _flowPercentile(uint8_t pct)
{
  if (_flow_sampleCount == 0) return 0.0f;

  // Copy to local buffer for sorting (avoids mutating the ring)
  float sorted[SAT_FLOW_SAMPLE_SIZE];
  uint16_t n = _flow_sampleCount;
  for (uint16_t i = 0; i < n; i++) {
    uint16_t src = (_flow_sampleHead + SAT_FLOW_SAMPLE_SIZE - n + i) % SAT_FLOW_SAMPLE_SIZE;
    sorted[i] = _flow_samples[src];
  }

  // Insertion sort — O(n^2) but n<=SAT_FLOW_SAMPLE_SIZE, runs once at cycle end
  for (uint16_t i = 1; i < n; i++) {
    float key = sorted[i];
    int16_t j = (int16_t)i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }

  // Index for requested percentile (0-based, clamped)
  uint16_t idx = (uint16_t)((uint32_t)pct * (n - 1) / 100);
  if (idx >= n) idx = n - 1;
  return sorted[idx];
}

//=== Tail-window percentile (Task #590) ===
// Same insertion-sort approach as _flowPercentile(), but uses the 1Hz-gated
// tail ring buffer so only the final SAT_TAIL_SAMPLE_SIZE seconds of the
// cycle are considered.
static float _tailPercentile(uint8_t pct)
{
  if (_tail_sampleCount == 0) return 0.0f;

  float sorted[SAT_TAIL_SAMPLE_SIZE];
  uint16_t n = _tail_sampleCount;
  for (uint16_t i = 0; i < n; i++) {
    uint16_t src = (_tail_sampleHead + SAT_TAIL_SAMPLE_SIZE - n + i) % SAT_TAIL_SAMPLE_SIZE;
    sorted[i] = _tail_samples[src];
  }

  for (uint16_t i = 1; i < n; i++) {
    float key = sorted[i];
    int16_t j = (int16_t)i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }

  uint16_t idx = (uint16_t)((uint32_t)pct * (n - 1) / 100);
  if (idx >= n) idx = n - 1;
  return sorted[idx];
}

//=== Cycle kind (TASK-1387): thermo-nova cycles/tracker.py:101-108 and _determine_cycle_kind :314-324 ===
// Inputs are counted at 1 Hz in satCycleSample():
//   DHW = slave DHW-mode bit, Statusflags bit 2 (device_state.hot_water_active,
//         coordinator/serial/__init__.py:57-59), tracker.py:103;
//   CH  = master CH-enable bit, Statusflags bit 8 (device_state.central_heating = coordinator.active,
//         coordinator/__init__.py:136, serial/__init__.py:53-55), counted only without DHW, tracker.py:104.
// The integer cross-multiplication is the same strict comparison as tracker.py's float fractions.
static SATCycleKind _cycleDetectKind()
{
  const uint64_t n = _cycle_totalSamples;
  if (n == 0) return SAT_CK_UNKNOWN;
  const uint64_t dhw = _cycle_dhwSamples;
  const uint64_t ch  = _cycle_chSamples;
  if (dhw * 5 > n * 4 && ch * 5 < n) return SAT_CK_DHW;    // tracker.py:315-316 dhw > 0.8 and ch < 0.2
  if (ch * 5 > n * 4 && dhw * 5 < n) return SAT_CK_CH;     // tracker.py:318-319 ch > 0.8 and dhw < 0.2
  if (dhw * 10 > n && ch * 10 > n)   return SAT_CK_MIXED;  // tracker.py:321-322 dhw > 0.1 and ch > 0.1
  return SAT_CK_UNKNOWN;                                   // tracker.py:324
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
  state.sat.iLastCycleEndMs = millis();
  state.sat.fCycleMaxFlow = maxFlow;
  state.sat.fCycleOvershootSec = overshootSec;
  state.sat.fLastCycleDuration = durationSec;
  state.sat.eLastCycleKind = kind;
  // CH/DHW fractions as tracker.py:106-107 (fraction_space_heating counts CH without DHW, so the two
  // need not add up to 1)
  if (_cycle_totalSamples > 0) {
    state.sat.fLastCycleFractionDHW = (float)_cycle_dhwSamples / (float)_cycle_totalSamples;
    state.sat.fLastCycleFractionCH  = (float)_cycle_chSamples  / (float)_cycle_totalSamples;
  } else {
    state.sat.fLastCycleFractionDHW = 0.0f;
    state.sat.fLastCycleFractionCH  = 0.0f;
  }

  // Update EMA fractions
  _ema_overshootFrac = EMA_ALPHA * (cls == SAT_CYCLE_OVERSHOOT ? 1.0f : 0.0f)
                     + (1.0f - EMA_ALPHA) * _ema_overshootFrac;
  _ema_underheatFrac = EMA_ALPHA * ((cls == SAT_CYCLE_UNDERHEAT || cls == SAT_CYCLE_UNDERHEAT_PWM) ? 1.0f : 0.0f)
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

//=== Classify a completed cycle (TASK-1387, Sergeant D entry 55: match classifier.py exactly) ===
// Port of thermo-nova cycles/classifier.py (pin 7beaaf5), rules in the same order; "cls:N" below is
// that file's line. Firmware deviations, each decided or planned:
//   - cold setpoint satGetColdSetpoint() (28.2 radiators / 21 underfloor, entries 7 and 11), not
//     COLD_SETPOINT 22;
//   - overshoot margin settings.sat.fOvershootMargin (2.0, `>=`, entries 15 and 37), not
//     OVERSHOOT_MARGIN_CELSIUS 3.0 (cycles/const.py:10);
//   - PWM underheat is labelled SAT_CYCLE_UNDERHEAT_PWM, continuous underheat SAT_CYCLE_UNDERHEAT;
//   - a missing percentile (thermo-nova: p50/p90 None) has these stand-ins: controlSp <= 0 for the
//     control-setpoint error (cls:71-72), requestedSp <= 0 for the requested-setpoint error (cls:42-43);
//   - both setpoints are one value per cycle (controlSp = state.sat.fFinalSetpoint at flame-on,
//     requestedSp = _sat_requestedSetpoint at flame-off), so percentile(flow - sp) = tailPxx - sp.
//     thermo-nova uses the per-sample setpoints (tracker.py:267-271); that port is TASK-1405;
//   - fewer than 3 samples is recorded as INSUFFICIENT; tracker.py:98-100 drops such a cycle;
//   - pwmIdle comes from satPwmIsIdle(), an approximation of pwm.py's IDLE status;
//   - the tail percentiles come from the last 180 s ring without tracker.py's warm-up exclusion
//     (tail_start = max(start + min(60, 0.25 * duration), end - 180), tracker.py:113-116), so a cycle
//     shorter than 240 s still has its first seconds in tailP50/tailP90.
// The kind is _cycleDetectKind(); dhwInTail is "a DHW sample in the tail" (tail_metrics.
// hot_water_active_fraction > 0, tracker.py:294-296), with the warm-up exclusion applied.
static SATCycleClass _cycleClassify(float durationSec, uint32_t sampleCount, SATCycleKind kind,
                                    bool dhwInTail, bool pwmEnabled, bool pwmIdle, bool pwmOn,
                                    float onTimeSec, float tailP90, float tailP50,
                                    float controlSp, float requestedSp)
{
  // cls:19-20 (and the firmware's < 3 samples, see above)
  if (durationSec <= 0.0f || sampleCount < 3) return SAT_CYCLE_INSUFFICIENT;

  // cls:22-23
  if (kind == SAT_CK_UNKNOWN) return SAT_CYCLE_UNCERTAIN;

  // cls:25-26
  if (dhwInTail) return SAT_CYCLE_UNCERTAIN;

  // cls:28-29
  if (pwmEnabled && pwmIdle) return SAT_CYCLE_UNCERTAIN;

  const float coldSp = satGetColdSetpoint();

  // cls:31-32 -> _classify_pwm, cls:40-66
  if (pwmEnabled) {
    // cls:42-43: flow_requested_setpoint_error p50/p90 missing
    if (requestedSp <= 0.0f) return SAT_CYCLE_UNCERTAIN;
    // cls:45: p90 of the requested-setpoint error <= UNDERSHOOT_MARGIN_CELSIUS (-3.0)
    const float reqErrP90 = tailP90 - requestedSp;
    if (reqErrP90 <= -SAT_UNDERSHOOT_MARGIN_C) {
      // cls:48-49: the effective setpoint is the requested setpoint (never missing past cls:42-43 here)
      if (requestedSp < coldSp) return SAT_CYCLE_UNCERTAIN;  // cls:51-52
      return SAT_CYCLE_UNDERHEAT_PWM;                        // cls:54
    }
    // cls:56-64: the short-cycle test only while the PWM ON phase runs at cycle end
    if (pwmOn) {
      if (onTimeSec <= 0.0f) return SAT_CYCLE_SHORT;               // cls:57-61
      if (durationSec < onTimeSec * 0.8f) return SAT_CYCLE_SHORT;  // cls:63-64
    }
    return SAT_CYCLE_GOOD;                                         // cls:66 (no OVERSHOOT for PWM)
  }

  // cls:34-35 -> _classify_continuous, cls:69-90
  // cls:71-72: flow_control_setpoint_error p50/p90 missing
  if (controlSp <= 0.0f) return SAT_CYCLE_UNCERTAIN;
  const float ctrlErrP90 = tailP90 - controlSp;
  const float ctrlErrP50 = tailP50 - controlSp;
  // cls:74, 77-78: overshoot is tested first
  if (ctrlErrP90 >= settings.sat.fOvershootMargin) return SAT_CYCLE_OVERSHOOT;
  // cls:75, 80-88
  if (ctrlErrP50 <= -SAT_UNDERSHOOT_MARGIN_C) {
    // cls:81-83: requested setpoint, else the device setpoint (the control setpoint)
    const float effSp = (requestedSp > 0.0f) ? requestedSp : controlSp;
    if (effSp < coldSp) return SAT_CYCLE_UNCERTAIN;   // cls:85-86
    return SAT_CYCLE_UNDERHEAT;                       // cls:88
  }
  return SAT_CYCLE_GOOD;                              // cls:90 returns None -> cls:37 GOOD
}

//=== Called on each flame status change ===
void satCycleOnFlameChange(bool flameOn)
{
  uint32_t now = millis();

  if (flameOn && !_cycle_flameOn) {
    // Flame just turned ON — start new cycle
    // TASK-891.4 (AC#5): demand-gated off-with-demand duration. The flame-off gap that
    // just ended only counts if heat demand was present (CH on, not DHW, setpoint above
    // flow temp), mirroring Python CycleTracker._compute_off_with_demand_duration.
    {
      float offSec = (_cycle_lastFlameOffMs > 0) ? (float)(now - _cycle_lastFlameOffMs) / 1000.0f : 0.0f;
      bool demandPresent = (!state.sat.bDhwActive)
                        && ((OTcurrentSystemState.Statusflags & 0x02) != 0)  // Bit 1 = CH active
                        && (state.sat.fFinalSetpoint > satGetFlowTemp());
      state.sat.fOffWithDemandSec = demandPresent ? offSec : 0.0f;
    }
    _cycle_flameOn = true;
    _cycle_flameOnStartMs = now;
    _cycle_maxFlowTemp = satGetFlowTemp();
    _cycle_minFlowTemp = satGetFlowTemp();
    _cycle_setpointAtStart = state.sat.fFinalSetpoint;
    _cycle_overshootSec = 0.0f;
    _cycle_lastSampleMs = now;
    // A flame-on edge is not a PWM start: the per-hour counter holds PWM ON-phase starts only (TASK-1401)
    state.sat.iCyclesThisHour = _hourCountGet(now);
    SATDebugTf(PSTR("SAT cycle: flame ON flow=%.1f sp=%.1f pwm starts/hr=%u\r\n"),
               satGetFlowTemp(), _cycle_setpointAtStart,
               (unsigned)state.sat.iCyclesThisHour);
    satNarratef_P(PSTR("Flame lit: flow %.0f\xc2\xb0""C, setpoint %.0f\xc2\xb0""C"),
                  satGetFlowTemp(), state.sat.fFinalSetpoint);
    {
      static char _wsMsg[80];
      snprintf_P(_wsMsg, sizeof(_wsMsg), PSTR("{\"type\":\"status\",\"msg\":\"Heating active, setpoint %.1f deg C\"}"), _cycle_setpointAtStart);
      sendWebSocketJSON(_wsMsg);
    }
    // Reset per-cycle flow sample buffer for p90/p10 classification (Task #225)
    _flow_sampleHead  = 0;
    _flow_sampleCount = 0;
    // Seed with current boiler temp so we have at least one sample
    _flow_samples[_flow_sampleHead] = satGetFlowTemp();
    _flow_sampleHead = (_flow_sampleHead + 1) % SAT_FLOW_SAMPLE_SIZE;
    _flow_sampleCount = 1;
    // Reset tail ring buffer (Task #590: 180s end-of-cycle window)
    memset(_tail_samples, 0, sizeof(_tail_samples));
    _tail_sampleHead  = 0;
    _tail_sampleCount = 0;
    _tail_lastSampleMs = now;
    // Reset the cycle kind / DHW-in-tail counters (TASK-1387)
    _cycle_dhwSamples    = 0;
    _cycle_chSamples     = 0;
    _cycle_totalSamples  = 0;
    _cycle_lastDhwMs     = 0;
    // Reset flow-return delta accumulator (Task #227)
    _cycle_sumFlowRetDelta = 0.0f;
    _cycle_deltasamples    = 0;
    // Reset per-cycle shape accumulators (TASK-891.4 AC#4)
    _cycle_timeInBandSec               = 0.0f;
    _cycle_totalOvershootSec           = 0.0f;
    _cycle_overshootStreakSec          = 0.0f;
    _cycle_timeToFirstOvershootSec     = -1.0f;
    _cycle_timeToSustainedOvershootSec = -1.0f;
  }
  else if (!flameOn && _cycle_flameOn) {
    // Flame just turned OFF — complete the cycle
    _cycle_flameOn = false;
    _cycle_flameOffStartMs = now;
    _cycle_phase = SAT_CP_COOLDOWN;
    _cycle_phaseStartMs = now;

    float durationSec = (float)(now - _cycle_flameOnStartMs) / 1000.0f;
    SATDebugTf(PSTR("SAT cycle: flame OFF dur=%.0fs maxFlow=%.1f\r\n"),
               durationSec, _cycle_maxFlowTemp);
    {
      static char _wsMsg[72];
      if (isnan(OTcurrentSystemState.Tr)) {
        strlcpy_P(_wsMsg, PSTR("{\"type\":\"status\",\"msg\":\"Heating off, room temp unknown\"}"), sizeof(_wsMsg));
      } else {
        snprintf_P(_wsMsg, sizeof(_wsMsg), PSTR("{\"type\":\"status\",\"msg\":\"Heating off, room %.1f deg C\"}"), OTcurrentSystemState.Tr);
      }
      sendWebSocketJSON(_wsMsg);
    }
    // Compute p90/p10 from collected flow samples (Task #225)
    float p90 = (_flow_sampleCount >= 10) ? _flowPercentile(90) : _cycle_maxFlowTemp;
    float p10 = (_flow_sampleCount >= 10) ? _flowPercentile(10) : _cycle_minFlowTemp;
    // Task #590: tail-window percentiles (last 180s at 1Hz, or full buffer if shorter).
    // A cycle that self-corrected should not be classified OVERSHOOT from its early peak.
    float tailP90 = (_tail_sampleCount >= 10) ? _tailPercentile(90) : p90;
    float tailP10 = (_tail_sampleCount >= 10) ? _tailPercentile(10) : p10;
    float tailP50 = (_tail_sampleCount >= 10) ? _tailPercentile(50) : (p90 + p10) * 0.5f;
    SATDebugTf(PSTR("SAT cycle classify: fullP90=%.1f tailP90=%.1f tailP50=%.1f tailP10=%.1f tailN=%u\r\n"),
               p90, tailP90, tailP50, tailP10, (unsigned)_tail_sampleCount);
    // Classifier inputs (TASK-1387, cycles/classifier.py). Snapshot the PID requested setpoint at
    // flame-off (it drifts across a multi-minute cycle). pwm_state at cycle end: enabled = PWM mode,
    // status IDLE = satPwmIsIdle(), ON = the PWM ON phase (bPwmFlameRequested), on_time_seconds =
    // the last PWM on-time.
    float requestedSp = _sat_requestedSetpoint;   // includes the solar offset, as thermo-nova requested_setpoint (TASK-1323)
    bool  pwmEnabled  = (state.sat.eControlMode == SAT_MODE_PWM);
    float onTimeSec   = (float)satPwmLastOnTimeMs() / 1000.0f;
    // hot_water_active_fraction > 0 over the tail (tracker.py:294-296). The tail is every sample at or
    // after tail_start = max(start + min(60, 0.25 * duration), end - 180) (tracker.py:113-116, :237-241),
    // so a DHW sample in the warm-up of a short cycle does not count. The tail is a suffix of the
    // cycle, so the last DHW sample decides. In seconds after flame-on: offset >= max(warm-up, dur - 180).
    const float tailStartSec = fmaxf(fminf(60.0f, 0.25f * durationSec), durationSec - 180.0f);
    bool  dhwInTail   = (_cycle_dhwSamples > 0) &&
                        ((float)_cycle_lastDhwMs / 1000.0f >= tailStartSec);
    SATCycleClass cls = _cycleClassify(durationSec, _cycle_totalSamples, _cycleDetectKind(),
                                       dhwInTail, pwmEnabled, satPwmIsIdle(),
                                       state.sat.bPwmFlameRequested, onTimeSec,
                                       tailP90, tailP50, _cycle_setpointAtStart, requestedSp);
    _cycleRecord(cls, durationSec, _cycle_maxFlowTemp, _cycle_overshootSec);

    // Expose the 2nd error metric (AC#1) + shape metrics (AC#4) for state/JSON.
    state.sat.fCycleReqSetpointError         = tailP90 - requestedSp;
    state.sat.fCycleTimeInBandSec            = _cycle_timeInBandSec;
    state.sat.fCycleTotalOvershootSec        = _cycle_totalOvershootSec;
    state.sat.fCycleTimeToFirstOvershoot     = _cycle_timeToFirstOvershootSec;
    state.sat.fCycleTimeToSustainedOvershoot = _cycle_timeToSustainedOvershootSec;
    {
      // Map the cycle class enum to a plain word for the observer.
      static const char *const _satCycleWord[] = { "none", "GOOD", "overshoot",
                                                   "underheat", "short", "uncertain",
                                                   "underheat_pwm", "insufficient" };
      const char *verdict = ((uint8_t)cls < (sizeof(_satCycleWord)/sizeof(_satCycleWord[0])))
                            ? _satCycleWord[(uint8_t)cls] : "?";
      satNarratef_P(PSTR("Flame off: cycle %s, %.0fs on, peak flow %.0f\xc2\xb0""C"),
                    verdict, durationSec, _cycle_maxFlowTemp);
    }

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

      // Mirror into the reduced-resolution 24h daily window (TASK-891.4 AC#5)
      _dailyRecord(now, onMs, offMs, cls, durationSec);
    }

    // Remember this flame-off edge for the next cycle's off-with-demand gating (AC#5)
    _cycle_lastFlameOffMs = now;

    SATDebugTf(PSTR("SAT cycle #%d: class=%d dur=%.0fs maxFlow=%.1f p90=%.1f p10=%.1f overshoot=%.0fs totOvershoot=%.0fs inBand=%.0fs\r\n"),
            state.sat.iCycleCount, (int)cls, durationSec,
            _cycle_maxFlowTemp, p90, p10, _cycle_overshootSec, _cycle_totalOvershootSec, _cycle_timeInBandSec);
  }
}

//=== Called periodically to sample flow temp during active cycle ===
void satCycleSample()
{
  if (!_cycle_flameOn) return;

  uint32_t now = millis();
  float flowTemp = satGetFlowTemp();

  if (flowTemp > _cycle_maxFlowTemp) _cycle_maxFlowTemp = flowTemp;
  if (flowTemp < _cycle_minFlowTemp) _cycle_minFlowTemp = flowTemp;

  // Accumulate flow-return delta for 4-hour window record (Task #227)
  // Tret may be 0 when return temp sensor is absent; only accumulate when plausible.
  float retTemp = satGetReturnTemp();
  if (retTemp > 10.0f && retTemp < 100.0f) {
    _cycle_sumFlowRetDelta += (flowTemp - retTemp);
    _cycle_deltasamples++;
  }

  // Flow sample rings, both 1Hz-gated so they span time, not loop iterations:
  // the per-cycle p90/p10 ring (Task #225) covers the last SAT_FLOW_SAMPLE_SIZE s of the
  // cycle, the tail ring (Task #590) the last SAT_TAIL_SAMPLE_SIZE s (TASK-1269).
  uint32_t nowMs = millis();
  if (nowMs - _tail_lastSampleMs >= 1000UL) {
    _flow_samples[_flow_sampleHead] = flowTemp;
    _flow_sampleHead = (_flow_sampleHead + 1) % SAT_FLOW_SAMPLE_SIZE;
    if (_flow_sampleCount < SAT_FLOW_SAMPLE_SIZE) _flow_sampleCount++;

    _tail_samples[_tail_sampleHead] = flowTemp;
    _tail_sampleHead = (_tail_sampleHead + 1) % SAT_TAIL_SAMPLE_SIZE;
    if (_tail_sampleCount < SAT_TAIL_SAMPLE_SIZE) _tail_sampleCount++;
    _tail_lastSampleMs = nowMs;

    // Cycle kind and DHW-in-tail inputs, counted in the same 1 Hz gate as the tail ring
    // (TASK-1387; tracker.py:103-104 counts per sample):
    // DHW = Statusflags bit 2 (slave DHW mode), CH = Statusflags bit 8 (master CH enable) without DHW.
    // SAT simulation has no OT status: the simulated DHW draw (bDhwActive, forced by the simulation)
    // stands in for bit 2, and the CH enable SAT commands stands in for bit 8 (CS above the minimum
    // setpoint: the CH rule sends CH=0 exactly with CS=10, satChEnableDecide; the simulated boiler
    // also only fires CH above it).
    bool dhwSample, chSample;
    if (settings.sat.bSimulation) {
      dhwSample = state.sat.bDhwActive;
      chSample  = state.sat.fFinalSetpoint > SAT_MIN_SETPOINT;
    } else {
      const uint16_t sf = OTcurrentSystemState.Statusflags;
      dhwSample = (sf & 0x0004) != 0;
      chSample  = (sf & 0x0100) != 0;
    }
    _cycle_totalSamples++;
    if (dhwSample) {
      _cycle_dhwSamples++;
      _cycle_lastDhwMs = nowMs - _cycle_flameOnStartMs;
    } else if (chSample) {
      _cycle_chSamples++;
    }

    if (++_flow_logTick >= 30) {   // one debug line per 30 samples (30 s)
      _flow_logTick = 0;
      SATDebugTf(PSTR("SAT sample: flow=%.1f n=%u sp=%.1f os=%.0fs\r\n"),
                 flowTemp, (unsigned)_flow_sampleCount, _cycle_setpointAtStart, _cycle_overshootSec);
    }
  }

  // Track overshoot seconds: time flow temp is above setpoint + margin
  float elapsed = (float)(now - _cycle_lastSampleMs) / 1000.0f;
  if (flowTemp > _cycle_setpointAtStart + settings.sat.fOvershootMargin) {
    _cycle_overshootSec += elapsed;
  }

  // --- Shape metrics (TASK-891.4 AC#4) — accumulated over flow-control setpoint error,
  //     post-warmup (Python _build_cycle_shape_metrics uses observation_start after the
  //     effective warmup). Streaming approximation: skip the first SAT_SHAPE_WARMUP_SEC.
  float sinceOnSec = (float)(now - _cycle_flameOnStartMs) / 1000.0f;
  if (sinceOnSec >= SAT_SHAPE_WARMUP_SEC) {
    float ctrlErr = flowTemp - _cycle_setpointAtStart;
    if (fabsf(ctrlErr) <= SAT_IN_BAND_MARGIN_C) {
      _cycle_timeInBandSec += elapsed;               // time_in_band_seconds
    }
    if (ctrlErr >= settings.sat.fOvershootMargin) {  // >= overshoot margin (Python OVERSHOOT_MARGIN_CELSIUS, >=3 to count)
      _cycle_totalOvershootSec += elapsed;           // total_overshoot_seconds
      if (_cycle_timeToFirstOvershootSec < 0.0f) {
        _cycle_timeToFirstOvershootSec = sinceOnSec; // time_to_first_overshoot
      }
      _cycle_overshootStreakSec += elapsed;
      if (_cycle_timeToSustainedOvershootSec < 0.0f &&
          _cycle_overshootStreakSec >= SAT_OVERSHOOT_SUSTAIN_SEC) {
        _cycle_timeToSustainedOvershootSec = sinceOnSec; // time_to_sustained_overshoot (first >=60s run)
      }
    } else {
      _cycle_overshootStreakSec = 0.0f;              // streak broken
    }
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
bool satCycleCheckAutoSwitch(float requestedSetpoint)
{
  uint32_t now = millis();
  float flowTemp = satGetFlowTemp();
  // The requested (PID) setpoint, not the final one: the continuous clamp keeps the final setpoint within
  // a few degrees of the flow, so comparing against it would almost never see an overshoot (thermo-nova
  // compares flow with the requested setpoint, heating_control.py _maybe_enable_pwm_on_sustained_overshoot).
  float setpoint = requestedSetpoint;

  // --- DHW post-overshoot guard: track end of DHW heating ---
  if (state.sat.bDhwActive) {
    _sustain_dhwEndMs = now; // Keep refreshing while DHW is active
  }

  // --- Overshoot detection (continuous → PWM), Sergeant D entry 41 ---
  // Timed from the first qualifying sample, as thermo-nova (heating_control.py:293-304, helpers.py:51-56):
  // that sample only starts the timer, and the switch comes once the overshoot has lasted 60 s of wall
  // clock. A sample that does not qualify, or any call outside CONTINUOUS (thermo-nova: PWM enabled,
  // :264-266), clears the start. Counted in millis(), so it does not depend on the control interval.
  // Exception: the test is strict (flow > setpoint + margin); thermo-nova also counts equality (it resets
  // only on flow < threshold, :289). Entry 37 decided >= for the cycle classifier only (open question Q43).
  // The switch also resets the underheat and saturation timers, as thermo-nova (:305).
  if (state.sat.eControlMode == SAT_MODE_CONTINUOUS) {
    // Skip overshoot→PWM switch while DHW is active or within 300s of DHW end
    bool dhwGuardActive = state.sat.bDhwActive ||
                          (now - _sustain_dhwEndMs < SAT_DHW_OVERSHOOT_GUARD_MS);
    if (!dhwGuardActive && flowTemp > setpoint + settings.sat.fOvershootMargin) {
      if (!_sustain_overshootRunning) {
        _sustain_overshootRunning = true;
        _sustain_overshootStartMs = now;
      } else if ((uint32_t)(now - _sustain_overshootStartMs) >= (uint32_t)(SAT_OVERSHOOT_SUSTAIN_SEC * 1000.0f)) {
        SATDebugTf(PSTR("SAT: sustained overshoot for %lus (flow=%.1f > sp=%.1f + %.1f), switching to PWM mode\r\n"),
                   (unsigned long)((now - _sustain_overshootStartMs) / 1000UL), flowTemp, setpoint,
                   settings.sat.fOvershootMargin);
        satSetControlMode(SAT_MODE_PWM);
        satCycleResetSustainGuards();   // _reset_pwm_disable_guards() after pwm.enable() (heating_control.py:304-305)
        _sustain_lastCheckMs = now;
        return true;
      }
    } else {
      _sustain_overshootRunning = false;
    }
  } else {
    _sustain_overshootRunning = false;
  }

  // The underheat and saturation timers below still add the time since the previous call.
  float dt = (float)(now - _sustain_lastCheckMs) / 1000.0f;
  _sustain_lastCheckMs = now;

  SATDebugTf(PSTR("SAT autoswitch: mode=%d flow=%.1f sp=%.1f os=%lus uh=%.0fs\r\n"),
             (int)state.sat.eControlMode, flowTemp, setpoint,
             _sustain_overshootRunning ? (unsigned long)((now - _sustain_overshootStartMs) / 1000UL) : 0UL,
             _sustain_underheatSec);

  if (dt <= 0.0f || dt > 60.0f) return false; // Skip on first call or large gaps

  // --- Underheat detection (PWM → continuous) ---
  // Both checks only run in the PWM ON phase (thermo-nova _maybe_disable_pwm_runtime returns and resets
  // the guards while the PWM status is not ON): in the OFF phase the flow is low by design.
  if (state.sat.eControlMode == SAT_MODE_PWM && !state.sat.bPwmFlameRequested) {
    _sustain_underheatSec  = 0.0f;
    _sustain_saturationSec = 0.0f;
  }
  if (state.sat.eControlMode == SAT_MODE_PWM && state.sat.bPwmFlameRequested) {
    if (flowTemp < setpoint - SAT_UNDERSHOOT_MARGIN_C) {
      _sustain_underheatSec += dt;
    } else {
      _sustain_underheatSec = 0.0f;
    }
    if (_sustain_underheatSec >= SAT_UNDERHEAT_SUSTAIN_SEC) {
      SATDebugTln(F("SAT: sustained underheat detected, switching to continuous mode"));
      satSetControlMode(SAT_MODE_CONTINUOUS);
      _sustain_underheatSec = 0.0f;
      return true;
    }

    // --- Saturation detection (PWM → continuous): PWM off_time stays 0 too long ---
    // Matches Python pwm.state.off_time_seconds == 0 sustained for SATURATION_SUSTAIN_SECONDS:
    // the duty calc wants the boiler running continuously, so PWM mode gives no benefit.
    // satPwmLastOffTimeMs() is recorded by satApplyPWM() earlier in the same control cycle.
    if (satPwmLastOffTimeMs() == 0) {
      _sustain_saturationSec += dt;
    } else {
      _sustain_saturationSec = 0.0f;
    }
    if (_sustain_saturationSec >= SAT_SATURATION_SUSTAIN_SEC) {
      SATDebugTln(F("SAT: PWM saturation (off_time=0 sustained), switching to continuous mode"));
      satSetControlMode(SAT_MODE_CONTINUOUS);
      _sustain_saturationSec = 0.0f;
      return true;
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
  SATDebugTf(PSTR("SAT countClass: class=%d count=%u total=%u\r\n"),
             (int)cls, (unsigned)count, (unsigned)_cycleHistoryCount);
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

//=== Heating Curve Daily Median Recommendation (Task #228) ===
//
// Algorithm:
//   - Once per day: compute the median of intra-day (room - target) error samples.
//     Error = -(state.sat.fError)  because fError = target - room.
//   - Store the daily median for the last 7 days in a ring buffer (7 floats = 28 bytes).
//   - Recommendation:
//       INCREASE  if median < -0.5 C for 3+ consecutive days  (room too cold)
//       DECREASE  if median > +0.5 C for 3+ consecutive days  (room too warm)
//       HOLD      otherwise
//   - Resets to "insufficient" when SAT is disabled or room sensor unavailable.
//   - Daily error ring buffer persists to LittleFS (/sat_hcr.json) so the recommendation
//     survives reboots.

//--- Compute median of the intra-day sample buffer (insertion-sort on local copy) ---
// sorted[] is static to avoid a large stack frame: on ESP32 HCR_INTRADAY_SIZE=1440 (5760 bytes).
// This function runs once per day and is not re-entrant, so static is safe.
static float _hcrIntraMedian()
{
  if (_hcr_sCount == 0) return 0.0f;
  static float sorted[HCR_INTRADAY_SIZE];
  uint16_t n = _hcr_sCount;
  for (uint16_t i = 0; i < n; i++) {
    uint16_t src = (_hcr_sHead + HCR_INTRADAY_SIZE - n + i) % HCR_INTRADAY_SIZE;
    sorted[i] = _hcr_samples[src];
  }
  // Insertion sort — n <= HCR_INTRADAY_SIZE, runs once per day
  for (uint16_t i = 1; i < n; i++) {
    float key = sorted[i];
    int16_t j  = (int16_t)i - 1;
    while (j >= 0 && sorted[j] > key) { sorted[j + 1] = sorted[j]; j--; }
    sorted[j + 1] = key;
  }
  // Median: lower-middle for even n
  return sorted[n / 2];
}

//--- Save HCR state to LittleFS (called on day-commit) ---
void satHCRSaveState()
{
  // create=true makes the /sat folder: a fresh filesystem image has none, and without
  // it this open fails and the recommendation never survives a restart (TASK-1286).
  File f = LittleFS.open(FPSTR(SAT_HCR_FILE), "w", true);
  if (!f) return;
  // Format: {"ts":T,"n":N,"h":H,"d":[v0,...,vN-1],"sn":SC,"s":[s0,...,sSC-1]}
  // Written chunk-by-chunk to avoid a large stack buffer.
  time_t ts = time(nullptr);
  char hdr[48];
  snprintf_P(hdr, sizeof(hdr), PSTR("{\"ts\":%lu,\"n\":%u,\"h\":%u,\"d\":["),
             (unsigned long)ts, (unsigned)_hcr_count, (unsigned)_hcr_head);
  f.print(hdr);
  for (uint8_t i = 0; i < HCR_DAYS; i++) {
    char fBuf[8];
    dtostrf(_hcr_dailyMedian[i], 1, 2, fBuf);
    f.print(fBuf);
    if (i < HCR_DAYS - 1) f.print(F(","));
  }
  // Intraday samples: cap at 96 to bound file size on all platforms
  uint16_t saveSamples = (_hcr_sCount > 96) ? 96 : _hcr_sCount;
  char shdr[24];
  snprintf_P(shdr, sizeof(shdr), PSTR("],\"sn\":%u,\"s\":["), (unsigned)saveSamples);
  f.print(shdr);
  for (uint16_t i = 0; i < saveSamples; i++) {
    uint16_t src = (uint16_t)((_hcr_sHead + HCR_INTRADAY_SIZE - saveSamples + i) % HCR_INTRADAY_SIZE);
    char fBuf[8];
    dtostrf(_hcr_samples[src], 1, 2, fBuf);
    f.print(fBuf);
    if (i < saveSamples - 1) f.print(F(","));
  }
  f.print(F("]}"));
  f.close();
  SATDebugTf(PSTR("SAT HCR: saved %u days %u intraday samples\r\n"),
             (unsigned)_hcr_count, (unsigned)_hcr_sCount);
}

//--- Load HCR state from LittleFS (called from satCycleInit when NTP is valid) ---
void satHCRLoadState()
{
  satMigrateFile(SAT_HCR_FILE_OLD, SAT_HCR_FILE);
  File f = LittleFS.open(FPSTR(SAT_HCR_FILE), "r");
  if (!f) return;
  // File now includes intraday samples; max size on ESP8266 ~878 bytes.
  // Static buffer: persists in BSS, not re-entrant, called once at boot.
  static char buf[960];
  size_t len = f.readBytes(buf, sizeof(buf) - 1);
  buf[len] = 0;
  f.close();

  char* p;
  unsigned n = 0, h = 0;
  if ((p = strstr(buf, "\"n\":")) != nullptr) n = (unsigned)atoi(p + 4);
  if ((p = strstr(buf, "\"h\":")) != nullptr) h = (unsigned)atoi(p + 4);
  if (n > HCR_DAYS) n = HCR_DAYS;
  if (h >= HCR_DAYS) h = 0;
  _hcr_count = (uint8_t)n;
  _hcr_head  = (uint8_t)h;

  // Parse daily median array
  p = strstr(buf, "\"d\":[");
  if (p) {
    p += 5;
    for (uint8_t i = 0; i < HCR_DAYS; i++) {
      _hcr_dailyMedian[i] = strtof(p, &p);
      if (*p == ',') p++;
    }
  }

  // Restore intraday samples (Task #237)
  unsigned sn = 0;
  if ((p = strstr(buf, "\"sn\":")) != nullptr) sn = (unsigned)atoi(p + 5);
  p = strstr(buf, "\"s\":[");
  if (p && sn > 0) {
    p += 5;
    uint16_t limit = (sn > HCR_INTRADAY_SIZE) ? HCR_INTRADAY_SIZE : (uint16_t)sn;
    _hcr_sHead  = 0;
    _hcr_sCount = 0;
    for (uint16_t i = 0; i < limit; i++) {
      float v = strtof(p, &p);
      _hcr_samples[_hcr_sHead] = v;
      _hcr_sHead = (_hcr_sHead + 1) % HCR_INTRADAY_SIZE;
      if (_hcr_sCount < HCR_INTRADAY_SIZE) _hcr_sCount++;
      if (*p == ',') p++;
    }
  }

  SATDebugTf(PSTR("SAT HCR: loaded %u days, %u intraday samples\r\n"),
          (unsigned)_hcr_count, (unsigned)_hcr_sCount);
}

//--- Reset the daily median recommendation (called when SAT disabled) ---
void satHCRReset()
{
  memset(_hcr_dailyMedian, 0, sizeof(_hcr_dailyMedian));
  _hcr_head   = 0;
  _hcr_count  = 0;
  _hcr_sHead  = 0;
  _hcr_sCount = 0;
  strlcpy(state.sat.sHeatCurveRec, "insufficient", sizeof(state.sat.sHeatCurveRec));
}

//--- Collect an intra-day sample (call from control loop, ~every 15 min) ---
// room_error = room_temp - target_temp = -(state.sat.fError)
void satHCRAddSample()
{
  if (!state.sat.bActive) return;

  float roomError = -state.sat.fError;  // positive: room warmer than target

  _hcr_samples[_hcr_sHead] = roomError;
  _hcr_sHead = (_hcr_sHead + 1) % HCR_INTRADAY_SIZE;
  if (_hcr_sCount < HCR_INTRADAY_SIZE) _hcr_sCount++;
  SATDebugTf(PSTR("SAT HCR: sample err=%.2f n=%u\r\n"),
             roomError, (unsigned)_hcr_sCount);

  // Check for day boundary: commit previous day's data
  time_t nowTs = time(nullptr);
  if (nowTs < 1000000L) return;  // NTP not synced yet
  uint32_t dayNum = (uint32_t)(nowTs / 86400UL);

  if (_hcr_lastDayNum == 0) {
    // First call after boot: just record which day we are on
    _hcr_lastDayNum = dayNum;
    return;
  }

  if (dayNum > _hcr_lastDayNum) {
    // New day: commit the accumulated intra-day samples as a daily median
    if (_hcr_sCount >= 2) {
      float med = _hcrIntraMedian();
      _hcr_dailyMedian[_hcr_head] = med;
      _hcr_head = (_hcr_head + 1) % HCR_DAYS;
      if (_hcr_count < HCR_DAYS) _hcr_count++;
      SATDebugTf(PSTR("SAT HCR: day %lu committed median=%.2f (%u days stored)\r\n"),
              (unsigned long)dayNum, med, (unsigned)_hcr_count);
    }
    // Reset intra-day buffer for the new day
    _hcr_sHead  = 0;
    _hcr_sCount = 0;
    _hcr_lastDayNum = dayNum;

    // Compute and update the recommendation after each new day
    satHeatingCurveRecommendation();
    satHCRSaveState();
  }
}

//--- Compute the current heating curve recommendation from daily medians ---
// Returns a PSTR-safe const char* (stored in state.sat.sHeatCurveRec for MQTT/JSON).
const char* satHeatingCurveRecommendation()
{
  if (!state.sat.bActive) {
    strlcpy(state.sat.sHeatCurveRec, "insufficient", sizeof(state.sat.sHeatCurveRec));
    return state.sat.sHeatCurveRec;
  }

  if (_hcr_count < HCR_SUSTAIN_DAYS) {
    strlcpy(state.sat.sHeatCurveRec, "insufficient", sizeof(state.sat.sHeatCurveRec));
    return state.sat.sHeatCurveRec;
  }

  // Walk the last HCR_SUSTAIN_DAYS entries (newest-first) from the ring buffer
  // and count how many consecutive days exceed each threshold.
  uint8_t consecIncrease = 0;
  uint8_t consecDecrease = 0;
  for (uint8_t i = 0; i < HCR_SUSTAIN_DAYS; i++) {
    // Walk backwards from newest: index = (head - 1 - i) mod HCR_DAYS
    uint8_t idx = (_hcr_head + HCR_DAYS - 1 - i) % HCR_DAYS;
    float med   = _hcr_dailyMedian[idx];
    if (med < -HCR_THRESHOLD_C) consecIncrease++;  // room too cold
    if (med >  HCR_THRESHOLD_C) consecDecrease++;  // room too warm
  }

  const char* rec;
  if (consecIncrease == HCR_SUSTAIN_DAYS) {
    rec = "INCREASE";
  } else if (consecDecrease == HCR_SUSTAIN_DAYS) {
    rec = "DECREASE";
  } else {
    rec = "HOLD";
  }

  strlcpy(state.sat.sHeatCurveRec, rec, sizeof(state.sat.sHeatCurveRec));
  SATDebugTf(PSTR("SAT HCR: recommendation=%s (inc=%u dec=%u n=%u)\r\n"),
          rec, (unsigned)consecIncrease, (unsigned)consecDecrease, (unsigned)_hcr_count);
  if (consecIncrease == HCR_SUSTAIN_DAYS || consecDecrease == HCR_SUSTAIN_DAYS) {
    static char _wsMsg[80];
    snprintf_P(_wsMsg, sizeof(_wsMsg), PSTR("{\"type\":\"status\",\"msg\":\"Heating curve: %s gradient recommended\"}"), rec);
    sendWebSocketJSON(_wsMsg);
  }
  return state.sat.sHeatCurveRec;
}

//=== Cycle Window Persistence (Task #237) ===
// Persists the _win4h ring buffer to /sat/sat_cycles.json, capped at 60 entries.
// SAT_CYCLES_FILE is defined in SATcontrol.ino (compiled before SATcycles.ino).
static const uint32_t SAT_CYCLES_STALE_SEC = 14400UL; // 4h stale threshold

void satFlushCycleWindow()
{
  LittleFS.remove(FPSTR(SAT_CYCLES_FILE));
  _win4hHead  = 0;
  _win4hCount = 0;
  SATDebugTln(F("SAT: cycle window flushed"));
}

