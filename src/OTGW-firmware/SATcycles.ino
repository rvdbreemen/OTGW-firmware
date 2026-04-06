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

//=== Classify a completed cycle ===
static SATCycleClass _cycleClassify(float durationSec, float maxFlowTemp, float setpoint, float overshootSec)
{
  // Too short → short cycling
  if (durationSec < SAT_CYCLE_SHORT_DURATION_SEC) {
    return SAT_CYCLE_SHORT;
  }

  // Significant overshoot detected
  if (maxFlowTemp > setpoint + settings.sat.fOvershootMargin && overshootSec > 10.0f) {
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
    _cycle_dhwSamples = 0;
    _cycle_totalSamples = 0;
  }
  else if (!flameOn && _cycle_flameOn) {
    // Flame just turned OFF — complete the cycle
    _cycle_flameOn = false;
    _cycle_flameOffStartMs = now;
    _cycle_phase = SAT_CP_COOLDOWN;
    _cycle_phaseStartMs = now;

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

  // --- Overshoot detection (continuous → PWM) ---
  if (state.sat.eControlMode == SAT_MODE_CONTINUOUS) {
    if (flowTemp > setpoint + settings.sat.fOvershootMargin) {
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

