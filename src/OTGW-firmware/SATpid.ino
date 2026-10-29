/*
***************************************************************************
**  Module   : SATpid.ino
**  Description: PID controller for the SAT (Smart Autotune Thermostat)
**
**  Ported from SAT Python custom component (releases/thermo-nova) pid.py
**  Original SAT component by Alex Wijnholds (https://github.com/Alexwijn/SAT)
**  SAT concept and algorithm design by George Dellas
**
**  PID version 3 with automatic gain calculation, temperature-based
**  derivative (not error-based), deadband mode switching, and single
**  low-pass filtered derivative with magnitude cap.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: GNU GPLv3. See bottom of OTGW-firmware.h
***************************************************************************
*/

// --- PID Constants ---
static const float SAT_PID_DEADBAND_DEFAULT     = 0.1f;   // Matches Python DEADBAND=0.1 (const.py)
static const float SAT_PID_UPDATE_INTERVAL      = 60.0f;   // seconds (pid.py:20, PID_UPDATE_INTERVAL)
// Derivative alpha is now adaptive: alpha = dt / (PID_UPDATE_INTERVAL + dt)
static const float SAT_PID_DERIVATIVE_CAP       = 5.0f;    // Max raw derivative magnitude
static const float SAT_PID_AGGRESSION_V3        = 8400.0f;

// Underfloor divisor=4, radiator divisor=3 for Kp calculation
static const float SAT_PID_KP_DIVISOR_FLOOR     = 4.0f;
static const float SAT_PID_KP_DIVISOR_RAD       = 3.0f;

// --- PID Internal State (static, not exposed) ---
static float  _pid_lastError          = 0.0f;
static float  _pid_previousError      = 0.0f;
static float  _pid_integral           = 0.0f;
static float  _pid_rawDerivative      = 0.0f;
static float  _pid_lastRoomTemp       = 0.0f;
static float  _pid_lastCurveValue     = 0.0f;
static uint32_t _pid_lastDerivativeMs = 0;
static bool   _pid_initialized        = false;

// The PID moves on its own 60 s timer, not on the control tick (Sergeant D entry 42; thermo-nova
// climate.py:712-716, PID_UPDATE_INTERVAL = pid.py:20), plus one start-up step (climate.py:137, :143).
// The type is explicit: nothing re-sets this timer, and an untyped CHANGE_INTERVAL_SEC would turn it into
// SKIP_MISSED_TICKS (entry 109). SKIP_MISSED_TICKS: a loop stall gives one step, never a catch-up burst.
DECLARE_TIMER_SEC(timerSATPid, 60, SKIP_MISSED_TICKS);
// Set by satPidReset(): the next satPidService() call takes the start-up step and restarts the timer.
static bool   _pid_startupPending     = true;
// Set at boot and by satPidReset(): the first satPidOutput() leaves the D term out (entry 95 Q39b: D = 0 on the
// first tick after boot or SAT enable), also when satPidRestoreState() pre-loaded a raw derivative.
static bool   _pid_firstOutputPending = true;
// False at boot and after satPidReset(), true from the first valid room temperature on (entry 115). While it is
// false a missing room temperature keeps the restored integral and raw derivative (satPidCurveOnly()).
static bool   _pid_roomSeen           = false;
bool satPidRoomSeen() { return _pid_roomSeen; }

// When the PID first saw the current room temperature (TASK-1195). It stands in
// for HA's last_changed, by which pid.py times the derivative (climate.py:314).
// satPidReset() leaves it alone: it describes the sensor, not the controller,
// and pid.py's reset() keeps its timers as well.
static float    _pid_roomTempSeen      = NAN;
static uint32_t _pid_roomTempChangedMs = 0;

// The room temperature at the moment _pid_lastDerivativeMs marks: the two always
// move together, so the derivative measures one change over one time span. pid.py
// overwrites its last_temperature on every update, also when it skips a change
// that came within 60 s. A step that reaches the PID as several small changes
// (the TASK-894 room EMA spreads every sensor step over about 8 control ticks)
// then loses most of its delta while dt keeps running. On a step-wise sensor both
// give the same result (TASK-1195).
static float    _pid_derivRefTemp      = 0.0f;

//=== PID error ===
// target - room, rounded to 3 decimals the way pid.py rounds TemperatureState.error
// (temperature/state.py). Unrounded, 21.0f - 20.9f is 0.1000004 and an error of 0.1
// falls just outside the 0.1 deadband, where pid.py keeps it inside (TASK-1195).
// The zone PID (satZonePidAdvance) uses it too.
float satPidError(float target, float room)
{
  return roundf((target - room) * 1000.0f) / 1000.0f;
}

//=== Restore PID internal state from persistence (Task #589) ===
// Called from satLoadPidState() after staleness validation.
// Sets _pid_integral and _pid_rawDerivative directly so the very next
// satPidStep() call warm-starts instead of cold-starting from zero.
// satPidReset() leaves _pid_initialized false, so the first step after it still initialises
// timestamps and lastRoomTemp from live data — we only pre-load the
// accumulated values that take many minutes to rebuild.
void satPidRestoreState(float integral, float rawDerivative)
{
  _pid_integral      = integral;
  _pid_rawDerivative = rawDerivative;
  _pid_firstOutputPending = true;   // a restore landing after the first output still gives D = 0 on the next one (entry 95 Q39b)
  // A restore that lands late overwrites the live integral and raw derivative; it does not wait for a reset.
  state.sat.fPidI    = integral;
  state.sat.fRawDerivative = rawDerivative;
  SATDebugTf(PSTR("SAT: PID internal state pre-loaded (I=%.4f rawD=%.4f)\r\n"),
             integral, rawDerivative);
}

//=== Controller state for persistence (round 3) ===
// The held state itself, not the published copies: satPidOutput() publishes I = 0 and D = 0 while no error is held,
// which must never reach the file. lastError is 0 while none is held (NAN). D is kd * raw derivative, the term a
// restored state resumes from.
void satPidGetPersistState(float* integral, float* d, float* rawDerivative, float* lastError)
{
  *integral      = _pid_integral;
  *rawDerivative = _pid_rawDerivative;
  *d             = state.sat.fKd * _pid_rawDerivative;
  *lastError     = isnan(_pid_lastError) ? 0.0f : _pid_lastError;
}

//=== Integral-only Reset (debug tool) ====
void satResetIntegral()
{
  _pid_integral = 0.0f;
  state.sat.fPidI = 0.0f;
  DebugTln(F("SAT: PID integral reset"));
}

//=== PID Reset ===
void satPidReset()
{
  _pid_lastError          = 0.0f;
  _pid_previousError      = 0.0f;
  _pid_integral           = 0.0f;
  _pid_rawDerivative      = 0.0f;
  _pid_lastRoomTemp       = 0.0f;
  _pid_lastCurveValue     = 0.0f;
  _pid_lastDerivativeMs   = millis();
  _pid_initialized        = false;
  _pid_startupPending     = true;     // the next satPidService() call takes the start-up step (TASK-1368)
  _pid_firstOutputPending = true;     // the first output after the reset has D = 0
  _pid_roomSeen           = false;

  state.sat.fPidP = 0.0f;
  state.sat.fPidI = 0.0f;
  state.sat.fPidD = 0.0f;
  state.sat.fPidOutput = 0.0f;
  state.sat.fKp = 0.0f;
  state.sat.fKi = 0.0f;
  state.sat.fKd = 0.0f;
  state.sat.fRawDerivative = 0.0f;
}

//=== PID without a room temperature (TASK-1364, entries 38 and 46) ===
// Lost or missing room temperature: no error, so P, I and D are 0 and the output is the
// heating curve. Integral and raw derivative are reset (thermo-nova pid.reset(),
// pid.py:134-137), not frozen. _pid_initialized stays set, so the first step after the
// loss is a normal one. _pid_lastError = NAN marks "no held error": satPidOutput() then
// returns the curve alone (never NaN) until the next step stores a real error.
// Exception (entry 115): while no valid room temperature has been seen since the boot or the last satPidReset(),
// the integral and raw derivative restored from flash are kept (and shown), and only P and D are 0: the output is
// the curve, and satSavePidState() does not write (satPidRoomSeen()) so the restored state is not overwritten.
// state.sat.fError reads 0 meanwhile: no NaN reaches the publishers, the persisted state or the statistics.
float satPidCurveOnly(float heatingCurveValue)
{
  _pid_lastError     = NAN;
  if (_pid_roomSeen) {                  // a reading was lost: reset (entries 38, 46)
    _pid_integral      = 0.0f;
    _pid_rawDerivative = 0.0f;
    state.sat.fPidI = 0.0f;
    state.sat.fRawDerivative = 0.0f;
  }
  state.sat.fPidP = 0.0f;
  state.sat.fPidD = 0.0f;
  state.sat.fError = 0.0f;
  state.sat.fPidOutput = heatingCurveValue;
  return heatingCurveValue;
}

//=== Auto-Gain Calculation (Version 3) ===
// Automatic gains from the heating curve (thermo-nova pid.py automatic_gains=True).
static void _pidCalculateGains(float curveValue)
{
  float coeff = settings.sat.fHeatingCurveCoeff;
  // Underfloor uses the floor divisor, radiators the radiator divisor. (TASK-891.8: was keyed
  // on '==1'/radiators, which inverted the mapping; now via the heating-system axis.)
  float divisor = (satGetEffectiveHeatingSystem() == SAT_HSYS_UNDERFLOOR) ? SAT_PID_KP_DIVISOR_FLOOR : SAT_PID_KP_DIVISOR_RAD;

  float kp = (coeff * curveValue) / divisor;
  float ki = kp / SAT_PID_AGGRESSION_V3;
  float kd = 0.07f * SAT_PID_AGGRESSION_V3 * kp;

  state.sat.fKp = kp;
  state.sat.fKi = ki;
  state.sat.fKd = kd;
}

//=== Integral Update ===
// Per sergeantd / SAT Python (pid.py): integral is ONLY active INSIDE the
// deadband as a smooth compensator for external heat sources (sun, cooking,
// activity). Outside the deadband, the heating curve replaces the integral
// role. Clamp to [-curveValue, +curveValue] — symmetric (Python: clamp_to_range).
static void _pidUpdateIntegral(float error, float curveValue, bool force)
{
  float deadband = settings.sat.fDeadband;

  // Solar gain freeze (Task #23, TASK-1323): while solar gain is active and the freeze setting is on,
  // the whole integral update is skipped, including the reset outside the deadband. thermo-nova:
  // freeze_integral = solar_gain_snapshot.active and solar_gain.freeze_integral (climate.py:499-502),
  // pid.py:167-168 skips _update_integral(), whose reset is pid.py:187-189 (7beaaf5).
  if (state.sat.bSolarGainActive && settings.sat.bSolarFreezeIntegral) return;

  // Outside deadband: heating curve takes over, reset integral
  if (fabsf(error) > deadband) {
    _pid_integral = 0.0f;
    return;
  }

  // Inside deadband: integral acts as compensator
  if (state.sat.fKi < 1e-9f) return;

  // Accumulate: Ki * error * PID_UPDATE_INTERVAL (fixed 60s per SAT Python)
  _pid_integral += state.sat.fKi * error * SAT_PID_UPDATE_INTERVAL;

  // Clamp integral symmetrically to [-curveValue, +curveValue].
  // Python reference: clamp_to_range(integral, curve_value) → [-curve_value, +curve_value].
  // Negative accumulation is required when room temp overshoots target (mild weather):
  // the integral must be able to nudge flow setpoint below the heating curve.
  if (_pid_integral < -curveValue) _pid_integral = -curveValue;
  if (_pid_integral >  curveValue) _pid_integral =  curveValue;

  // Hard absolute cap — symmetric safety bound against runaway in both directions
  static const float SAT_PID_INTEGRAL_ABS_MAX = 20.0f;
  if (_pid_integral >  SAT_PID_INTEGRAL_ABS_MAX) _pid_integral =  SAT_PID_INTEGRAL_ABS_MAX;
  if (_pid_integral < -SAT_PID_INTEGRAL_ABS_MAX) _pid_integral = -SAT_PID_INTEGRAL_ABS_MAX;
}

//=== Derivative Update ===
// Per sergeantd / SAT Python (pid.py): temperature-based derivative with
// negative sign (rising temp = negative derivative = damping). Uses adaptive
// alpha for low-pass filter that scales with sample rate.
// Inside deadband: derivative FREEZES at last calculated value (persists as
// heating curve offset). Outside deadband: derivative actively updates.
static void _pidUpdateDerivative(float roomTemp, float error)
{
  float deadband = settings.sat.fDeadband;

  // The timer runs on when the room temperature changed, not on when the PID ran:
  // pid.py stamps every branch with the sensor's last_changed, so dt is the time
  // between two room-temperature changes however often the PID runs (TASK-1195).
  uint32_t changedMs = _pid_roomTempChangedMs;

  // Inside deadband: FREEZE derivative (keep last value), update timestamp only.
  // Uses the CURRENT error to exactly match Python pid.py:205 (abs(state.error) <= DEADBAND);
  // previously checked _pid_lastError, a 1-cycle lag at the deadband boundary. (TASK-893)
  if (fabsf(error) <= deadband) {
    // _pid_rawDerivative keeps its last value — intentional freeze
    _pid_lastDerivativeMs = changedMs;
    _pid_derivRefTemp     = roomTemp;
    return;
  }

  // No temperature change: just update timestamp. pid.py checks this before the
  // interval (pid.py:209-213), so an idle call cannot push the next step out.
  float tempDelta = roomTemp - _pid_derivRefTemp;
  if (fabsf(tempDelta) < 0.001f) {
    _pid_lastDerivativeMs = changedMs;
    _pid_derivRefTemp     = roomTemp;
    return;
  }

  float deltaTime = (float)(changedMs - _pid_lastDerivativeMs) / 1000.0f;

  // A change within the interval of the timer: skip it (pid.py: delta_time <= PID_UPDATE_INTERVAL).
  // Timer and reference stay put, so the next update takes this change along.
  if (deltaTime <= SAT_PID_UPDATE_INTERVAL) return;

  // Temperature-based derivative with NEGATIVE sign (SAT Python convention):
  // rising temp -> negative derivative -> reduces PID output (damping)
  float rawDeriv = -tempDelta / deltaTime;

  // Hard cap first (SAT Python applies cap before filter on extreme values)
  if (fabsf(rawDeriv) >= SAT_PID_DERIVATIVE_CAP) {
    if (rawDeriv > SAT_PID_DERIVATIVE_CAP)  rawDeriv = SAT_PID_DERIVATIVE_CAP;
    if (rawDeriv < -SAT_PID_DERIVATIVE_CAP) rawDeriv = -SAT_PID_DERIVATIVE_CAP;
    _pid_rawDerivative = rawDeriv;
    _pid_lastDerivativeMs = changedMs;
    _pid_derivRefTemp     = roomTemp;
    return;
  }

  // Adaptive alpha low-pass filter (SAT Python: alpha = dt / (interval + dt))
  // Longer dt -> alpha closer to 1.0 (trusts new reading more)
  float alpha = deltaTime / (SAT_PID_UPDATE_INTERVAL + deltaTime);
  float filtered = alpha * rawDeriv + (1.0f - alpha) * _pid_rawDerivative;

  // Final cap
  if (filtered > SAT_PID_DERIVATIVE_CAP) filtered = SAT_PID_DERIVATIVE_CAP;
  if (filtered < -SAT_PID_DERIVATIVE_CAP) filtered = -SAT_PID_DERIVATIVE_CAP;

  _pid_rawDerivative = filtered;
  _pid_lastDerivativeMs = changedMs;
  _pid_derivRefTemp     = roomTemp;
}

//=== Room temperature stamp ===
// Stamps the moment a new room temperature shows up. The control tick calls it every tick and the
// PID step calls it too, so the derivative timer keeps the resolution it had when the PID ran on the
// tick (TASK-1195); calls that see the same temperature leave the stamp alone.
void satPidNoteRoom(float roomTemp, uint32_t nowMs)
{
  if (!(fabsf(roomTemp - _pid_roomTempSeen) < 0.001f)) {   // true on the first call: Seen starts as NAN
    _pid_roomTempSeen      = roomTemp;
    _pid_roomTempChangedMs = nowMs;
  }
}

//=== One PID step (TASK-1368, entry 42) ===
// Moves the held state once: gains, integral, derivative, error. Run by satPidService() every 60 s
// and once at start-up, as thermo-nova pid.update() (pid.py:159-171): no time gate and no
// unchanged-error gate (entry 75 removes the 10 s gate, entry 42 the unchanged-error skip).
// Only the error, the integral, the raw derivative and the last temperature are held between steps;
// satPidOutput() builds the output from them.
void satPidStep(float roomTemp, float targetTemp, float heatingCurveValue, uint32_t nowMs)
{
  float error = satPidError(targetTemp, roomTemp);
  satPidNoteRoom(roomTemp, nowMs);
  // Marked here, not in satPidNoteRoom(): a reading the tick sees first must still trigger the immediate
  // first-reading step in satPidService() (entry 115).
  _pid_roomSeen = true;

  // Initialize on first step
  if (!_pid_initialized) {
    _pid_lastError        = error;
    _pid_previousError    = error;
    _pid_lastRoomTemp     = roomTemp;
    _pid_lastCurveValue   = heatingCurveValue;
    _pid_lastDerivativeMs = _pid_roomTempChangedMs;   // when the current temperature appeared
    _pid_derivRefTemp     = roomTemp;
    _pid_initialized      = true;
  }

  // Calculate gains based on current heating curve
  _pidCalculateGains(heatingCurveValue);

  // Update integral (force update on each PID cycle)
  _pidUpdateIntegral(error, heatingCurveValue, true);

  // Update derivative (temperature-based); pass the current error for the deadband freeze
  _pidUpdateDerivative(roomTemp, error);

  state.sat.fError = error;
  state.sat.fPidI = _pid_integral;
  state.sat.fRawDerivative = _pid_rawDerivative;

  // Update tracking variables
  _pid_previousError  = _pid_lastError;
  _pid_lastError      = error;
  _pid_lastRoomTemp   = roomTemp;
  _pid_lastCurveValue = heatingCurveValue;
}

//=== PID output (TASK-1368, entry 28) ===
// Computed on every read from the CURRENT curve, as thermo-nova pid.output (pid.py:126-132, read every
// loop at climate.py:375): curve + kp(curve) * held error + I + kd(curve) * held raw derivative.
// kp and kd follow the live curve (pid.py:60-93), so a changed target or outside temperature reaches the
// output on the next control tick, while error, integral and raw derivative stay as the last step left them.
// A held error of NAN (satPidCurveOnly(): no room temperature) gives the curve alone.
float satPidOutput(float heatingCurveValue)
{
  _pidCalculateGains(heatingCurveValue);
  float P = 0.0f;
  float I = 0.0f;
  float D = 0.0f;
  if (!isnan(_pid_lastError)) {
    P = state.sat.fKp * _pid_lastError;
    I = _pid_integral;
    if (!_pid_firstOutputPending) D = state.sat.fKd * _pid_rawDerivative;   // entry 95 Q39b: D = 0 on the first output with PID terms
    _pid_firstOutputPending = false;   // only an output that includes the PID terms counts; a curve-only output leaves it set
  }
  state.sat.fPidP = P;
  state.sat.fPidI = I;
  state.sat.fPidD = D;
  state.sat.fRawDerivative = _pid_rawDerivative;

  // PID output = heatingCurveValue + P + I + D (thermo-nova style)
  float output = heatingCurveValue + P + I + D;
  state.sat.fPidOutput = output;
  return output;
}

//=== PID service: the 60 s timer (TASK-1368, entry 42, entry 111) ===
// Called every loop pass of satControlLoopRun() while SAT is enabled and not tripped, flashing or
// waiting on a lost OpenTherm link. It sits before the control tick's early returns (hot water, boiler
// fault, thermal-safe fallback, summer simmer, valves closed): thermo-nova steps its PID on its own timer
// whatever the heating control does (climate.py:712-716), and Sergeant D chose the same (entry 111).
// It stops only when SAT stops; an OpenTherm loss turns SAT off (satDisable() -> satPidReset()), so the
// next pass after the link returns takes a start-up step again.
// Steps: one start-up step after satPidReset(), then one per timerSATPid expiry. No room temperature
// (entries 38, 46): no integration, curve only with P, I and D reset. The zone PIDs ride the same timer.
// While the room temperature is estimated (MQTT takeover) the thermal-estimation deadband widening applies to the
// main step and to the zone steps alike, as it did when both ran inside the control tick; satPidGatherInputs()
// returns 0 once the estimate is older than SAT_THERMAL_MAX_EST_MS, where the tick used to return before widening.
void satPidService(uint32_t nowMs)
{
  const bool startup = _pid_startupPending;
  const bool due = startup || DUE(timerSATPid);
  if (!due && _pid_roomSeen) return;
  if (startup) {
    _pid_startupPending = false;
    RESTART_TIMER(timerSATPid);
  }

  float room = 0.0f, target = 0.0f, curve = 0.0f, deadbandExtra = 0.0f;
  const bool roomValid = satPidGatherInputs(&room, &target, &curve, &deadbandExtra);
  if (!due) {
    // Entry 115: no valid room temperature seen since the boot or reset, so the restored state is held. The first
    // valid reading takes a step at once, as the start-up step would, and restarts the timer; until then the pass
    // between two timer expiries does nothing.
    if (!roomValid) return;
    RESTART_TIMER(timerSATPid);
  }
  const float savedDeadband = settings.sat.fDeadband;
  settings.sat.fDeadband += deadbandExtra;   // 0 without a valid room temperature
  if (roomValid) satPidStep(room, target, curve, nowMs);
  else           satPidCurveOnly(curve);
  satZonePidAdvanceAll();
  settings.sat.fDeadband = savedDeadband;
}
