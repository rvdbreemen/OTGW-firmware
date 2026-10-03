/*
***************************************************************************
**  Program  : test/host/test_sat_pid_derivative.cpp
**
**  TASK-1195 host harness: does a room-temperature step reach the SAT PID
**  derivative filter, and with which dt?
**
**  Code under test, cut out of the tested revision by
**  test_sat_pid_derivative.py and never copied by hand:
**    - SATpid.ino, the whole file                        sat_pid_slice.inc
**    - satGetEffectiveHeatingSystem() (SATcontrol.ino)   sat_hsys_slice.inc
**    - the zone PID step (SATcontrol.ino): SAT_MIN_SETPOINT, satZones[],
**      satZonePidExclude(), satZonePidStep()             sat_zone_slice.inc
**    - SATtypes.h itself, so every default in settings.sat and state.sat
**      is the firmware's own.
**  Platform shims: millis() reads a settable clock and the debug macros
**  print nothing. sat_pid_shim/Arduino.h stands in for <Arduino.h>.
**  Inputs, not code under test: satCalcHeatingCurve() returns CURVE and
**  satGetMaxSetpoint() returns 80, for the zone step.
**
**  Reference oracle, NOT code under test: PyPid transcribes the derivative
**  path of update() and _update_derivative() in pid.py:159-232
**  (other-projects/SAT-releases-thermo-nova, DEADBAND from const.py:16).
**  It gets the sensor's own change time, the way climate.py:314 passes HA's
**  last_changed, and runs every 60 s like control_pid (climate.py:713).
**
**  The dt the firmware used is recovered from the filter output, not read
**  from its timer: r1 = a*(-delta/dt) + (1-a)*r0 with a = dt/(I+dt) gives
**  r1 = (-delta + I*r0)/(I+dt), so dt = (-delta + I*r0)/r1 - I. That works
**  on every revision, however the code keeps time. Check H1 proves the
**  inversion on calls whose dt is known.
**
**  Output: a report, then one line per check: "CHECK <id> PASS|FAIL <detail>".
**  Exit code: 0 when every check passes, 1 otherwise.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#include "SATtypes.h"
#include <algorithm>
#include <cstdarg>
#include <vector>

// ---- platform shims ------------------------------------------------------------
static uint32_t g_millis = 0;
static inline uint32_t millis() { return g_millis; }
#define DebugTln(...)   ((void)0)
#define SATDebugTf(...) ((void)0)

struct HostSettings { SATSection        sat; } settings;   // settings.sat with the real defaults
struct HostState    { SATRuntimeSection sat; } state;      // state.sat with the real defaults

static const float    CURVE   = 40.0f;   // heating-curve value fed to the PIDs: it only scales the gains
static const float    BOILER  = 50.0f;
static const uint32_t BASE_MS = 1000000u;

// Inputs for the zone step, not code under test.
static float satCalcHeatingCurve(float, float) { return CURVE; }
static float satGetMaxSetpoint() { return 80.0f; }

#include "sat_hsys_slice.inc"   // satGetEffectiveHeatingSystem(), from SATcontrol.ino
#include "sat_pid_slice.inc"    // SATpid.ino
#include "sat_zone_slice.inc"   // the zone PID step, from SATcontrol.ino

// The room-temperature EMA that satControlLoop() runs before satPidUpdate()
// (TASK-894). The block is cut verbatim out of satControlLoop(); only this
// function shell is the harness's. It smooths roomTemp in place, like the loop.
static float roomEma(float roomTemp) {
#include "sat_ema_slice.inc"
  return roomTemp;
}

// ---- reference oracle: pid.py (thermo-nova). Not code under test. ---------------
struct PyPid {
  static constexpr double DEADBAND            = 0.1;   // const.py:16
  static constexpr double PID_UPDATE_INTERVAL = 60.0;  // pid.py:20
  static constexpr double DERIVATIVE_RAW_CAP  = 5.0;   // pid.py:19
  enum Outcome { INIT, FREEZE, NOCHANGE, TOOSOON, CAPPED, FILTERED };

  bool   hasLastTemp = false, hasTimer = false;
  double lastTemp = 0.0, timer = 0.0, raw = 0.0, lastDt = 0.0;

  // update(): _update_derivative(state), then self._last_temperature = state.current
  Outcome update(double current, double setpoint, double lastChanged) {
    Outcome o = derivative(current, setpoint, lastChanged);
    lastTemp = current;
    hasLastTemp = true;
    return o;
  }
  Outcome derivative(double current, double setpoint, double lastChanged) {
    double error = std::round((setpoint - current) * 1000.0) / 1000.0;  // TemperatureState.error
    if (!hasLastTemp || !hasTimer) { timer = lastChanged; hasTimer = true; return INIT; }
    if (std::fabs(error) <= DEADBAND) { timer = lastChanged; return FREEZE; }
    double dTemp = current - lastTemp;
    if (dTemp == 0.0) { timer = lastChanged; return NOCHANGE; }
    double dt = lastChanged - timer;
    if (dt <= PID_UPDATE_INTERVAL) return TOOSOON;
    lastDt = dt;
    double d = -dTemp / dt;
    if (std::fabs(d) >= DERIVATIVE_RAW_CAP) {
      raw = std::max(-DERIVATIVE_RAW_CAP, std::min(d, DERIVATIVE_RAW_CAP));
      timer = lastChanged;
      return CAPPED;
    }
    double alpha = dt / (PID_UPDATE_INTERVAL + dt);
    raw = std::max(-DERIVATIVE_RAW_CAP, std::min(alpha * d + (1.0 - alpha) * raw, DERIVATIVE_RAW_CAP));
    timer = lastChanged;
    return FILTERED;
  }
};
static const char* pyName(int o) {
  static const char* n[] = {"init", "freeze", "nochange", "toosoon", "capped", "filtered"};
  return (o >= 0 && o <= 5) ? n[o] : "-";
}

// ---- checks ---------------------------------------------------------------------
static int g_fail = 0;
static void check(const char* id, bool ok, const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  printf("CHECK %s %s %s\n", id, ok ? "PASS" : "FAIL", buf);
  if (!ok) g_fail++;
}

// ---- one firmware call, observed ------------------------------------------------
struct FwObs {
  bool   consumed = false;  // the call took a new room temperature into _pid_lastRoomTemp
  bool   reached  = false;  // ...and the derivative filter, or the cap, ran on it
  bool   capped   = false;
  float  delta = 0.0f;      // the temperature change the firmware saw
  float  r0 = 0.0f, r1 = 0.0f;
  double dt = NAN;          // dt the filter used, recovered from r0, r1 and delta
};

static FwObs fwCall(uint32_t t, float room, float target) {
  FwObs o;
  g_millis = t;
  const bool  wasInit  = _pid_initialized;   // false: this call initialises the PID
  const float lastRoom = _pid_lastRoomTemp;
  o.r0 = _pid_rawDerivative;
  satPidUpdate(room, target, CURVE, BOILER);
  o.r1 = _pid_rawDerivative;
  o.delta = room - lastRoom;
  o.consumed = wasInit && fabsf(o.delta) >= 0.001f && _pid_lastRoomTemp == room;
  o.reached  = o.consumed && o.r1 != o.r0;
  o.capped   = o.reached && fabsf(o.r1) >= SAT_PID_DERIVATIVE_CAP;
  if (o.reached && !o.capped) {
    const double I = (double)SAT_PID_UPDATE_INTERVAL;
    o.dt = (-(double)o.delta + I * (double)o.r0) / (double)o.r1 - I;
  }
  return o;
}

// ---- a stepped heat-up on the control-tick grid -----------------------------------
struct RunCfg {
  int    offsetS        = 0;      // phase of the steps against the control-tick grid
  bool   jitter         = false;  // 0..50 ms loop latency per tick (the grid holds: CATCH_UP_MISSED_TICKS)
  bool   targetWobble   = false;  // the target moves every tick (comfort offset, PV boost); the room does not
  int    resetAfterStep = 0;      // >0: satPidReset() on the first tick after that step was consumed
  int    nSteps         = 40;
  double stepPeriodS    = 300.0;
  double startTemp      = 16.0;
  double target         = 21.0;
  double stepC          = 0.1;
  bool   ema            = false;  // the raw sensor goes through the TASK-894 EMA first, as on the device
};

struct StepRec {
  bool   seen = false, reached = false;
  double dtFw = NAN;
  float  rawFw = 0.0f;
  bool   pySeen = false;
  int    pyOutcome = -1;
  double dtPy = NAN, rawPy = 0.0;
};

struct RunRes {
  std::vector<StepRec> steps;     // index = step number, 1..nSteps
  int    reached = 0, swallowed = 0;   // over steps 2..nSteps
  double dtMin = 1e9, dtMax = -1e9;    // over the reached steps 2..nSteps
  float  rawFinal = 0.0f;
  double rawPyFinal = 0.0;
  float  kdFinal = 0.0f;
  bool   spike = false;           // |rawDerivative| reached the cap
  bool   targetOnlyMoved = false; // the derivative moved on a call without a room-temperature change
  bool   resetZeroed = true;      // rawDerivative was 0 right after the reset tick
  int    callsReached = 0;        // calls that ran the filter, over the whole run
  double callDtMin = 1e9, callDtMax = -1e9;
  double rawMeanFw = 0.0, rawMeanPy = 0.0;   // mean over the ticks of the second half of the steps
};

static uint32_t stepMs(const RunCfg& c, int j) {
  return BASE_MS + (uint32_t)c.offsetS * 1000u + (uint32_t)(j * c.stepPeriodS * 1000.0);
}

static RunRes runScenario(const RunCfg& c) {
  satPidReset();
  PyPid py;
  RunRes r;
  r.steps.resize(c.nSteps + 1);
  const uint32_t tickS  = settings.sat.iControlInterval;
  const uint32_t tickMs = tickS * 1000u;
  const uint32_t endMs  = stepMs(c, c.nSteps) + (uint32_t)(2.0 * c.stepPeriodS * 1000.0);
  uint32_t lcg = 2463534242u ^ ((uint32_t)c.offsetS * 2654435761u) ^ (c.jitter ? 0x9E3779B9u : 0u);
  bool resetDone = false;
  double meanFw = 0.0, meanPy = 0.0;
  int    meanN = 0;

  for (uint32_t k = 0;; k++) {
    uint32_t lat = 0;
    if (c.jitter) { lcg = lcg * 1664525u + 1013904223u; lat = (lcg >> 16) % 51u; }
    const uint32_t t = BASE_MS + k * tickMs + lat;
    if (t > endMs) break;

    int n = 0;                                    // steps that happened by now
    for (int j = 1; j <= c.nSteps; j++) if (stepMs(c, j) <= t) n = j;
    const double roomD   = c.startTemp + c.stepC * n;
    const double targetD = c.target + (c.targetWobble ? (double)((int)(k % 3) - 1) * 0.05 : 0.0);

    bool resetNow = false;
    if (c.resetAfterStep > 0 && !resetDone && r.steps[c.resetAfterStep].seen) {
      satPidReset();
      resetDone = resetNow = true;
    }

    const float roomIn = c.ema ? roomEma((float)roomD) : (float)roomD;
    FwObs o = fwCall(t, roomIn, (float)targetD);
    if (o.reached) {
      r.callsReached++;
      if (!std::isnan(o.dt)) { r.callDtMin = std::min(r.callDtMin, o.dt); r.callDtMax = std::max(r.callDtMax, o.dt); }
    }
    if (n > c.nSteps / 2 && n <= c.nSteps) { meanFw += o.r1; meanPy += py.raw; meanN++; }
    if (o.consumed && n >= 1 && !r.steps[n].seen) {
      StepRec& s = r.steps[n];
      s.seen = true; s.reached = o.reached; s.dtFw = o.dt; s.rawFw = o.r1;
    }
    if (fabsf(o.r1) >= SAT_PID_DERIVATIVE_CAP) r.spike = true;
    if (!o.consumed && o.r1 != o.r0) r.targetOnlyMoved = true;
    if (resetNow && o.r1 != 0.0f) r.resetZeroed = false;

    if ((k * tickS) % 60u == 0u) {                // control_pid runs every 60 s
      const double lastChangedS = (n == 0) ? (BASE_MS / 1000.0 - 3600.0) : stepMs(c, n) / 1000.0;
      const bool sees = py.hasLastTemp && roomD != py.lastTemp;
      const PyPid::Outcome po = py.update(roomD, targetD, lastChangedS);
      if (sees && n >= 1 && !r.steps[n].pySeen) {
        StepRec& s = r.steps[n];
        s.pySeen = true; s.pyOutcome = po; s.rawPy = py.raw;
        s.dtPy = (po == PyPid::FILTERED || po == PyPid::CAPPED) ? py.lastDt : NAN;
      }
    }
  }
  for (int j = 2; j <= c.nSteps; j++) {
    const StepRec& s = r.steps[j];
    if (s.seen && s.reached) {
      r.reached++;
      if (!std::isnan(s.dtFw)) { r.dtMin = std::min(r.dtMin, s.dtFw); r.dtMax = std::max(r.dtMax, s.dtFw); }
    } else {
      r.swallowed++;
    }
  }
  r.rawFinal   = _pid_rawDerivative;
  r.rawPyFinal = py.raw;
  r.kdFinal    = state.sat.fKd;
  if (meanN > 0) { r.rawMeanFw = meanFw / meanN; r.rawMeanPy = meanPy / meanN; }
  return r;
}

static void printSteps(const char* title, const RunCfg& c, const RunRes& r) {
  printf("\n-- %s (offset %d s, jitter %s, target wobble %s)\n", title, c.offsetS,
         c.jitter ? "0-50 ms" : "off", c.targetWobble ? "on" : "off");
  printf("   step  change@s | firmware: reached      dt_s     raw       | pid.py: outcome     dt_s     raw\n");
  for (int j = 1; j <= c.nSteps; j++) {
    const StepRec& s = r.steps[j];
    printf("   %4d  %8.0f | %-8s %-8s %8.1f  %+.6f | %-9s %8.1f  %+.6f\n", j,
           (stepMs(c, j) - BASE_MS) / 1000.0,
           s.seen ? "seen" : "unseen", s.reached ? "yes" : "NO", s.dtFw, s.rawFw,
           pyName(s.pyOutcome), s.dtPy, s.rawPy);
  }
}

// ---- AC#3: the pid.py rules the fix must keep -------------------------------------
static void acThreeCases() {
  printf("\n== AC#3 cases (direct calls, dt set by the call times)\n");

  // C3 + H1: alpha = dt/(60+dt) on two changes 90 s and 110 s apart.
  satPidReset();
  fwCall(BASE_MS, 16.0f, 21.0f);
  FwObs a = fwCall(BASE_MS + 90000u, 16.1f, 21.0f);
  FwObs b = fwCall(BASE_MS + 200000u, 16.3f, 21.0f);
  const double e1 = (90.0 / 150.0) * (-(double)a.delta / 90.0);
  const double e2 = (110.0 / 170.0) * (-(double)b.delta / 110.0) + (60.0 / 170.0) * (double)a.r1;
  const bool c3 = a.reached && b.reached && std::fabs(a.r1 - e1) <= 1e-8 + 1e-5 * std::fabs(e1)
                                         && std::fabs(b.r1 - e2) <= 1e-8 + 1e-5 * std::fabs(e2);
  check("C3", c3, "alpha=dt/(60+dt): got %+.7f, %+.7f; expected %+.7f, %+.7f", a.r1, b.r1, e1, e2);
  check("H1", std::fabs(a.dt - 90.0) < 0.05 && std::fabs(b.dt - 110.0) < 0.05,
        "dt recovered from the filter output: %.3f s and %.3f s (known 90 and 110)", a.dt, b.dt);

  // C1: inside the deadband the derivative freezes at its last value.
  satPidReset();
  fwCall(BASE_MS, 16.0f, 21.0f);
  FwObs f1 = fwCall(BASE_MS + 90000u, 16.1f, 21.0f);
  FwObs f2 = fwCall(BASE_MS + 180000u, 16.2f, 16.25f);   // error 0.05
  check("C1", f1.reached && f2.consumed && !f2.reached && f2.r1 == f1.r1,
        "deadband freeze: before %+.7f, after a step at error 0.05 %+.7f", f1.r1, f2.r1);

  // C2/C2b: a raw slope at or beyond 5 per second is clamped, not filtered.
  satPidReset();
  fwCall(BASE_MS, 16.0f, 600.0f);
  FwObs c2 = fwCall(BASE_MS + 90000u, 511.0f, 600.0f);   // -495/90 = -5.5
  check("C2", c2.reached && c2.r1 == -5.0f, "cap on a -5.5/s slope: %+.4f (expected -5.0, unfiltered)", c2.r1);
  satPidReset();
  fwCall(BASE_MS, 600.0f, 1000.0f);
  FwObs c2b = fwCall(BASE_MS + 90000u, 105.0f, 1000.0f); // +495/90 = +5.5
  check("C2b", c2b.reached && c2b.r1 == 5.0f, "cap on a +5.5/s slope: %+.4f (expected +5.0, unfiltered)", c2b.r1);
}

// ---- the deadband boundary at an error of exactly 0.1 -----------------------------
// pid.py rounds the error to 3 decimals (TemperatureState.error), so 21.0 - 20.9 is
// 0.1 and inside the deadband: the integral keeps growing and the derivative freezes.
// Unrounded, 21.0f - 20.9f is 0.1000004: outside, the integral resets to 0.
static void boundaryCases() {
  printf("\n== deadband boundary: 21.0f - 20.9f = %.7f, deadband %.7f\n",
         (double)(21.0f - 20.9f), (double)settings.sat.fDeadband);

  // B1: primary PID integral.
  satPidReset();
  fwCall(BASE_MS, 20.95f, 21.0f);
  fwCall(BASE_MS + 90000u, 20.95f, 21.0f);               // error 0.05: inside, the integral grows
  const float iInside = state.sat.fPidI;
  fwCall(BASE_MS + 180000u, 20.9f, 21.0f);               // error 0.1 as a 0.1-resolution sensor gives it
  const float iEdge = state.sat.fPidI;
  check("B1", iInside > 0.0f && iEdge > iInside,
        "primary integral after error 0.05: %+.6f, after error 0.1: %+.6f (inside: it grows)", iInside, iEdge);

  // B1d: primary PID derivative freezes at error 0.1.
  satPidReset();
  fwCall(BASE_MS, 16.0f, 21.0f);
  FwObs a = fwCall(BASE_MS + 90000u, 16.1f, 21.0f);      // a real step: the derivative moves
  FwObs b = fwCall(BASE_MS + 180000u, 20.9f, 21.0f);     // error 0.1: inside, frozen
  check("B1d", a.reached && b.consumed && b.r1 == a.r1,
        "primary derivative before %+.7f, after a step to error 0.1 %+.7f (inside: frozen)", a.r1, b.r1);

  // B2: zone PID integral, through satZonePidStep().
  SATZoneState& z = satZones[0];
  z = SATZoneState();
  z.bOff = false; z.bRoomValid = true; z.bSpValid = true;
  z.fSetpoint = 21.0f;
  g_millis = BASE_MS;
  z.iLastUpdateMs = g_millis;
  z.fRoomTemp = 20.95f;
  satZonePidStep(0, 5.0f);                               // error 0.05: inside
  const float zInside = z.fPidIntegral;
  g_millis += 30000u;
  z.iLastUpdateMs = g_millis;
  z.fRoomTemp = 20.9f;
  satZonePidStep(0, 5.0f);                               // error 0.1
  const float zEdge = z.fPidIntegral;
  check("B2", zInside > 0.0f && zEdge > zInside,
        "zone integral after error 0.05: %+.6f, after error 0.1: %+.6f (inside: it grows)", zInside, zEdge);
}

// ---- zone PID Kp per heating system (TASK-1197) ------------------------------------
// pid.py kp(): 4 if UNDERFLOOR else 3 (pid.py:70), and so does _pidCalculateGains().
// With an error of 1.0 the zone integral is 0 (outside the deadband), so the zone
// output is curve + kp * 1.0 and gives kp directly.
static void zoneGainCases() {
  printf("\n== zone PID Kp per heating system\n");
  const uint8_t saved = settings.sat.iHeatingSystem;
  struct { uint8_t hsys; const char* name; float divisor; } cases[] = {
    {SAT_HSYS_AUTO, "auto", 3.0f}, {SAT_HSYS_RADIATORS, "radiators", 3.0f}, {SAT_HSYS_UNDERFLOOR, "underfloor", 4.0f}};
  bool ok = true;
  char detail[256];
  int  pos = 0;
  for (const auto& k : cases) {
    settings.sat.iHeatingSystem = k.hsys;
    SATZoneState& z = satZones[0];
    z = SATZoneState();
    z.bOff = false; z.bRoomValid = true; z.bSpValid = true;
    z.fSetpoint = 21.0f;
    z.fRoomTemp = 20.0f;                                 // error 1.0: outside the deadband
    g_millis = BASE_MS;
    z.iLastUpdateMs = g_millis;
    const float kp   = satZonePidStep(0, 5.0f) - CURVE;
    const float want = settings.sat.fHeatingCurveCoeff * CURVE / k.divisor;
    if (fabsf(kp - want) > 0.01f) ok = false;
    pos += snprintf(detail + pos, sizeof(detail) - pos, "%s kp %.2f (want %.2f); ", k.name, kp, want);
  }
  settings.sat.iHeatingSystem = saved;
  check("Z1", ok, "%s", detail);
}

int main() {
  printf("== TASK-1195 SAT PID derivative harness\n");
  printf("   settings.sat: fDeadband=%.3f iControlInterval=%u s bAutoGains=%d fHeatingCurveCoeff=%.2f\n",
         settings.sat.fDeadband, (unsigned)settings.sat.iControlInterval, (int)settings.sat.bAutoGains,
         settings.sat.fHeatingCurveCoeff);
  printf("   SATpid.ino: SAT_PID_UPDATE_INTERVAL=%.0f s SAT_PID_DERIVATIVE_CAP=%.1f\n",
         (double)SAT_PID_UPDATE_INTERVAL, (double)SAT_PID_DERIVATIVE_CAP);

  // AC#1: 0.1 C every 5 min, control interval from settings, phase swept.
  printf("\n== AC#1 sweep: 40 steps of +0.1 C every 300 s, step phase 0..29 s, loop latency off/on\n");
  int    reachMin = 1 << 30, reachMax = -1;
  double dtMin = 1e9, dtMax = -1e9, rawMin = 1e9, rawMax = -1e9, pyRawMin = 1e9, pyRawMax = -1e9;
  bool   allReach = true, allDt = true, allOracle = true, anySpike = false;
  float  kd = 0.0f;
  const double tick = settings.sat.iControlInterval;
  for (int jit = 0; jit <= 1; jit++) {
    for (int off = 0; off < 30; off++) {
      RunCfg c; c.offsetS = off; c.jitter = jit != 0;
      RunRes r = runScenario(c);
      if (off == 0 && jit == 0) printSteps("per step, aligned run", c, r);
      if (off == 17 && jit == 1) printSteps("per step, offset run", c, r);
      reachMin = std::min(reachMin, r.reached); reachMax = std::max(reachMax, r.reached);
      if (r.reached > 0) { dtMin = std::min(dtMin, r.dtMin); dtMax = std::max(dtMax, r.dtMax); }
      rawMin = std::min(rawMin, (double)r.rawFinal); rawMax = std::max(rawMax, (double)r.rawFinal);
      pyRawMin = std::min(pyRawMin, r.rawPyFinal); pyRawMax = std::max(pyRawMax, r.rawPyFinal);
      kd = r.kdFinal;
      anySpike |= r.spike;
      if (r.reached != c.nSteps - 1) allReach = false;
      for (int j = 2; j <= c.nSteps; j++)
        if (r.steps[j].reached && !(std::fabs(r.steps[j].dtFw - c.stepPeriodS) <= tick)) allDt = false;
      if (std::fabs(r.rawFinal - r.rawPyFinal) > 0.10 * std::fabs(r.rawPyFinal)) allOracle = false;
    }
  }
  printf("\n   over 60 runs, steps 2..40 (39 per run):\n");
  printf("   reached the filter: min %d, max %d of 39\n", reachMin, reachMax);
  printf("   dt the filter used: %.1f .. %.1f s (the room changed every 300 s)\n", dtMin, dtMax);
  printf("   final rawDerivative: firmware %+.6f .. %+.6f, pid.py %+.6f .. %+.6f\n", rawMin, rawMax, pyRawMin, pyRawMax);
  printf("   D term = Kd x raw with Kd %.0f: firmware %+.2f .. %+.2f C, pid.py %+.2f .. %+.2f C\n",
         kd, kd * rawMin, kd * rawMax, kd * pyRawMin, kd * pyRawMax);

  check("A2_reach", allReach, "every step 2..40 reached the filter in all 60 runs (min %d, max %d of 39)", reachMin, reachMax);
  check("A2_dt", allDt, "every reached step used dt within %.0f s of 300 s (seen %.1f .. %.1f s)", tick, dtMin, dtMax);
  check("A2_oracle", allOracle, "final rawDerivative within 10%% of pid.py in all runs (%+.6f .. %+.6f vs %+.6f .. %+.6f)",
        rawMin, rawMax, pyRawMin, pyRawMax);
  check("C7", !anySpike, "no run hit the derivative cap on a 0.1 C step");

  // C5: target-only changes (comfort offset, PV boost) between the steps.
  bool targetMoved = false, wobbleReach = true;
  int  wMin = 1 << 30, wMax = -1;
  for (int off = 0; off < 30; off++) {
    RunCfg c; c.offsetS = off; c.jitter = true; c.targetWobble = true;
    RunRes r = runScenario(c);
    targetMoved |= r.targetOnlyMoved;
    wMin = std::min(wMin, r.reached); wMax = std::max(wMax, r.reached);
    if (r.reached != c.nSteps - 1) wobbleReach = false;
  }
  check("C5a", !targetMoved, "a target change without a room change never moved the derivative (30 runs)");
  check("C5b", wobbleReach, "with the target moving every tick, every step 2..40 reached the filter (min %d, max %d of 39)", wMin, wMax);

  // C4: leaving the deadband. Target 21.05, room 20.5 -> 21.5 in 0.1 steps:
  // steps 6 and 7 (21.0, 21.1) are inside, step 8 (21.2, error -0.15) leaves it.
  {
    RunCfg c; c.offsetS = 11; c.jitter = true; c.nSteps = 10; c.startTemp = 20.5; c.target = 21.05;
    RunRes r = runScenario(c);
    printSteps("C4 deadband crossing", c, r);
    const StepRec& s = r.steps[8];
    check("C4", s.reached && std::fabs(s.dtFw - s.dtPy) <= tick,
          "first step out of the deadband: firmware %s dt %.1f s, pid.py %s dt %.1f s",
          s.reached ? "reached," : "SWALLOWED,", s.dtFw, pyName(s.pyOutcome), s.dtPy);
  }

  // C6: satPidReset() in the middle of a heat-up.
  {
    RunCfg c; c.offsetS = 7; c.jitter = true; c.resetAfterStep = 10;
    RunRes r = runScenario(c);
    check("C6", !r.spike && r.resetZeroed, "after satPidReset() the derivative restarts at 0 without a spike");
    const StepRec& s = r.steps[11];
    check("C6b", s.reached && std::fabs(s.dtFw - c.stepPeriodS) <= tick,
          "the first step after the reset reached the filter with dt %.1f s (%s)", s.dtFw,
          s.reached ? "reached" : "SWALLOWED");
  }

  // R1: a sensor that changes more often than the PID runs. A change that comes within
  // 60 s of the timer is skipped; the derivative must then take it along with the next
  // update instead of dropping it, so the slope comes out right. pid.py drops it (it
  // overwrites last_temperature on every update) and comes out low here.
  {
    RunCfg c; c.offsetS = 5; c.jitter = true; c.nSteps = 240; c.stepPeriodS = 30.0; c.stepC = 0.01;
    RunRes r = runScenario(c);
    const double slope = -0.01 / 30.0;
    check("R1", std::fabs(r.rawFinal - slope) <= 0.10 * std::fabs(slope),
          "fast sensor, +0.01 C every 30 s: final rawDerivative %+.6f, true slope %+.6f (pid.py %+.6f)",
          r.rawFinal, slope, r.rawPyFinal);
  }

  // E1: the device path. satControlLoop() smooths the raw sensor with the TASK-894 EMA
  // before satPidUpdate(), so every step reaches the PID as several small changes.
  // pid.py gets the raw HA entity (climate.py:271-279 applies no smoothing), so it is
  // the measure of what the derivative should come to.
  {
    double fwMin = 1e9, fwMax = -1e9, pyMean = 0.0;
    int    callsMin = 1 << 30, callsMax = -1;
    bool   allE1 = true;
    for (int jit = 0; jit <= 1; jit++) {
      for (int off = 0; off < 30; off++) {
        RunCfg c; c.offsetS = off; c.jitter = jit != 0; c.ema = true;
        RunRes r = runScenario(c);
        fwMin = std::min(fwMin, r.rawMeanFw); fwMax = std::max(fwMax, r.rawMeanFw);
        pyMean = r.rawMeanPy;
        callsMin = std::min(callsMin, r.callsReached); callsMax = std::max(callsMax, r.callsReached);
        if (std::fabs(r.rawMeanFw - r.rawMeanPy) > 0.10 * std::fabs(r.rawMeanPy)) allE1 = false;
      }
    }
    // No dt range here: the dt recovery takes _pid_lastRoomTemp as the reference, which
    // only holds when no other change came in between, and through the EMA they do.
    printf("\n== E1 device path: +0.1 C every 300 s through the TASK-894 EMA, 60 runs\n");
    printf("   firmware: %d..%d filter runs per heat-up\n", callsMin, callsMax);
    check("E1", allE1, "mean rawDerivative over steps 21-40 within 10%% of pid.py on the raw sensor in all runs"
          " (firmware %+.6f .. %+.6f, pid.py %+.6f)", fwMin, fwMax, pyMean);

    // The same through the EMA with steps every 150 s, as the bench run feeds them
    // (report only: the bench prediction).
    double bMin = 1e9, bMax = -1e9;
    for (int off = 0; off < 30; off++) {
      RunCfg c; c.offsetS = off; c.jitter = true; c.ema = true; c.nSteps = 16; c.stepPeriodS = 150.0;
      RunRes r = runScenario(c);
      bMin = std::min(bMin, r.rawMeanFw); bMax = std::max(bMax, r.rawMeanFw);
    }
    printf("   bench prediction, +0.1 C every 150 s through the EMA: mean rawDerivative over steps 9-16"
           " %+.6f .. %+.6f; true slope %+.6f\n", bMin, bMax, -0.1 / 150.0);
  }

  acThreeCases();
  boundaryCases();
  zoneGainCases();

  printf("\n== %s: %d check(s) failed\n", g_fail ? "FAIL" : "PASS", g_fail);
  return g_fail ? 1 : 0;
}
