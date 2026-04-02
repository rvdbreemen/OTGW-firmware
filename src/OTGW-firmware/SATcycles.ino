/*
***************************************************************************
**  Module   : SATcycles.ino
**  Description: Cycle tracker and classifier for SAT
**
**  Ported from SAT releases/thermo-nova cycles/ package
**  (https://github.com/Alexwijn/SAT) with permission from the authors.
**
**  Monitors flame ON/OFF transitions, builds per-cycle metrics,
**  and classifies each completed cycle to drive PWM auto-switching.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

// --- Cycle Constants ---
static const uint8_t  SAT_CYCLE_HISTORY_SIZE       = 8;
static const float    SAT_CYCLE_SHORT_DURATION_SEC  = 60.0f;   // Cycles shorter than this = SHORT_CYCLING
static const float    SAT_OVERSHOOT_MARGIN_C        = 3.0f;    // Flow temp margin above setpoint for overshoot
static const float    SAT_OVERSHOOT_SUSTAIN_SEC     = 60.0f;   // Sustained overshoot before PWM switch
static const float    SAT_UNDERSHOOT_MARGIN_C       = 2.0f;    // Below setpoint margin for underheat
static const float    SAT_UNDERHEAT_SUSTAIN_SEC     = 180.0f;  // Sustained underheat before continuous switch
static const float    SAT_SATURATION_SUSTAIN_SEC    = 300.0f;  // Sustained saturation before continuous switch

// --- Current Cycle State ---
static bool     _cycle_flameOn          = false;
static uint32_t _cycle_flameOnStartMs   = 0;
static uint32_t _cycle_flameOffStartMs  = 0;
static float    _cycle_maxFlowTemp      = 0.0f;
static float    _cycle_minFlowTemp      = 999.0f;
static float    _cycle_setpointAtStart  = 0.0f;
static float    _cycle_overshootSec     = 0.0f;
static uint32_t _cycle_lastSampleMs     = 0;

// --- Sustained State Detection ---
static float    _sustain_overshootSec   = 0.0f;
static float    _sustain_underheatSec   = 0.0f;
static float    _sustain_saturationSec  = 0.0f;
static uint32_t _sustain_lastCheckMs    = 0;

// --- Cycle History ---
struct SATCycleRecord {
  SATCycleClass eClass;
  float         fDurationSec;
  float         fMaxFlowTemp;
  float         fOvershootSec;
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
}

//=== Record a completed cycle into history ===
static void _cycleRecord(SATCycleClass cls, float durationSec, float maxFlow, float overshootSec)
{
  _cycleHistory[_cycleHistoryIdx].eClass = cls;
  _cycleHistory[_cycleHistoryIdx].fDurationSec = durationSec;
  _cycleHistory[_cycleHistoryIdx].fMaxFlowTemp = maxFlow;
  _cycleHistory[_cycleHistoryIdx].fOvershootSec = overshootSec;
  _cycleHistoryIdx = (_cycleHistoryIdx + 1) % SAT_CYCLE_HISTORY_SIZE;
  if (_cycleHistoryCount < SAT_CYCLE_HISTORY_SIZE) _cycleHistoryCount++;

  state.sat.iCycleCount++;
  state.sat.eLastCycleClass = cls;
  state.sat.fCycleMaxFlow = maxFlow;
  state.sat.fCycleOvershootSec = overshootSec;
}

//=== Classify a completed cycle ===
static SATCycleClass _cycleClassify(float durationSec, float maxFlowTemp, float setpoint, float overshootSec)
{
  // Too short → short cycling
  if (durationSec < SAT_CYCLE_SHORT_DURATION_SEC) {
    return SAT_CYCLE_SHORT;
  }

  // Significant overshoot detected
  if (maxFlowTemp > setpoint + SAT_OVERSHOOT_MARGIN_C && overshootSec > 10.0f) {
    return SAT_CYCLE_OVERSHOOT;
  }

  // Flow never reached setpoint → underheat
  if (maxFlowTemp < setpoint - SAT_UNDERSHOOT_MARGIN_C) {
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
  }
  else if (!flameOn && _cycle_flameOn) {
    // Flame just turned OFF — complete the cycle
    _cycle_flameOn = false;
    _cycle_flameOffStartMs = now;

    float durationSec = (float)(now - _cycle_flameOnStartMs) / 1000.0f;
    SATCycleClass cls = _cycleClassify(durationSec, _cycle_maxFlowTemp,
                                        _cycle_setpointAtStart, _cycle_overshootSec);
    _cycleRecord(cls, durationSec, _cycle_maxFlowTemp, _cycle_overshootSec);

    DebugTf(PSTR("SAT cycle #%d: class=%d dur=%.0fs maxFlow=%.1f overshoot=%.0fs\r\n"),
            state.sat.iCycleCount, (int)cls, durationSec,
            _cycle_maxFlowTemp, _cycle_overshootSec);
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

  // Track overshoot seconds: time flow temp is above setpoint + margin
  float elapsed = (float)(now - _cycle_lastSampleMs) / 1000.0f;
  if (flowTemp > _cycle_setpointAtStart + SAT_OVERSHOOT_MARGIN_C) {
    _cycle_overshootSec += elapsed;
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

  // --- Overshoot detection (continuous → PWM) ---
  if (state.sat.eControlMode == SAT_MODE_CONTINUOUS) {
    if (flowTemp > setpoint + SAT_OVERSHOOT_MARGIN_C) {
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
