// SATcurveTune.h - TASK-1525 (ADR-205) heating curve self-tune: constants and types.
// TASK-1796 (ADR-228, Sergeant D entries 303, 304): the signal is split per control tick into a warm-up and a floating
// phase, and the coefficient changes only when both phases say the same direction.
// Kept in a header, included from SATtypes.h, because the Arduino builder hoists function prototypes above
// any type defined later in a .ino (the functions live in SATcontrol.ino, satCurveTune blocks).
// tests/test_sat_curve_selftune.py compiles this header together with those blocks.
#pragma once
#include <stdint.h>

// TASK-1889 (ADR-260, entries 329 and 367, Q-1889a option A by directive): the tune reacts in hours, not days. A tuner
// "day" is now a decision block of ONE hour that counts when its hour counts; the window holds the last 12 counted blocks
// (12 counted hours), every counted hour with a full window is a decision, a change empties the window, and the step is
// 0.05. The identifiers keep the word "day" for the decision block so the reviewed window code of ADR-228 stays as it is.
// The #ifndef guards let tests/test_sat_curve_selftune.py compile this same code with the pre-ADR-260 timing (24 h days,
// 3-day window, 0.1) so its day-based scenarios keep covering the unchanged window logic; the firmware never defines them.
#ifndef SAT_CT_WINDOW_DAYS
#define SAT_CT_WINDOW_DAYS      12     // counted decision blocks (= counted hours) the decision looks at (ADR-260)
#endif
#ifndef SAT_CT_MIN_CLEAN_HOURS
#define SAT_CT_MIN_CLEAN_HOURS  1      // counted hours a decision block needs to count (ADR-260; was 6 per 24 h day, entry 164)
#endif
#ifndef SAT_CT_HOURS_PER_DAY
#define SAT_CT_HOURS_PER_DAY    1      // tuner hours per decision block (ADR-260; was 24)
#endif
#define SAT_CT_SUN_MAX_DEG      12.0f  // clean hours need the sun below this (entry 161)
#define SAT_CT_BAND_C           1.0f   // decision threshold, symmetric (entry 165)
#ifndef SAT_CT_STEP
#define SAT_CT_STEP             0.05f  // fixed step (ADR-260; was 0.1, entry 165)
#endif
#define SAT_CT_COEFF_MIN        0.1f   // coefficient setting range (settingStuff.ino, ADR-198)
#define SAT_CT_COEFF_MAX        5.0f
#define SAT_CT_DEFAULT_MIN_MOD  5.0f   // minimum modulation when MsgID 15 was never seen (entry 174)
#define SAT_CT_MIN_TICK_FRACTION  0.75f  // an hour counts only with this share of its expected control ticks (hour length / control interval)
// TASK-1932 (Sergeant D entries 340, 343; ADR-234): an hour counts when its clean ticks (those that passed the per-tick
// exclusions of satTuneTickReason) are at least this share, in percent, of the EXPECTED ticks (hour length / control
// interval: 108 of 120 at 30 s); a missed tick counts against the hour like an excluded one (entry 343, Q-340a B). An hour
// that logged more ticks than expected still needs only this share of the EXPECTED ticks (entry 369, Q-343a B).
// Its excluded ticks stay out of the hour mean and the phase counts. SAT_CT_MIN_TICK_FRACTION (few_ticks) stays.
#define SAT_CT_MIN_CLEAN_PCT      90u
// Phase split (ADR-228, entry 304, Q-1787d A by our reading): a tick is floating when |error| <= this band, warm-up when
// |error| > it. The value is thermo-nova DEADBAND (const.py:16, used at pid.py:187 `> DEADBAND` and pid.py:205 `<= DEADBAND`,
// pin 7beaaf5), the same 0.1 C as SAT_PID_DEADBAND in SATtypes.h (not included here: this header comes first).
// "Outside" is both sides of the target, so an overshoot tick counts as warm-up (entry 306, Q-1787f A).
#define SAT_CT_PHASE_BAND_C     0.1f
// A phase without a tick in the window does not veto: the phase with data decides alone; with data in both phases both must
// agree (Sergeant D entry 305, Q-1787e B). 1 = the as-first-built hold (a phase without data holds).
#define SAT_CT_MISSING_PHASE_VETO 0
// The floating phase votes only with at least this much data (its signal ticks times the control interval) over the
// counted days of the window; with less, the warm-up phase decides alone, as for a phase without data (Sergeant D
// entries 313 and 315, Q-1796a B and Q-1796a2 B: 1 hour). The warm-up phase has no such minimum (entry 313 names floating).
#define SAT_CT_FLOAT_MIN_MS     3600000UL
#define SAT_CT_PHASE_SPLIT      1      // marks the phase-split tuner (the host test sets SatTuneTickIn.error only with it)
#ifndef SAT_CT_FILE_VERSION
#define SAT_CT_FILE_VERSION     3      // window file format: 3 = one-hour blocks (ADR-260); files without "v":3 are dropped
#endif
// An hour with a lost room temperature (signal 0, curve only) is not counted: reason no_room (Sergeant D entry 225,
// Q-1525a A, TASK-1559).

enum SatTuneReason : uint8_t {
  SATT_NONE = 0, SATT_NOT_SAT, SATT_DHW, SATT_NO_SUN, SATT_SUN, SATT_PWM, SATT_FLAME_OFF, SATT_MIN_MOD,
  SATT_COEFF_CHANGED, SATT_NO_TICKS, SATT_NOT_CONTROLLING, SATT_FEW_TICKS, SATT_NO_ROOM,
  SATT_DIAG_EVENT,   // TASK-1882 (entry 358): a no-ignition or heating-failure event was active
  SATT_COUNT   // number of reasons, SATT_NONE included (size of the per-reason counters)
};
// TASK-1881: per-reason counters of excluded hours (telemetry only, the decision does not read them). The host test sets its
// counter checks only with this marker, so the tuner from before TASK-1881 still compiles in the old-vs-fix run.
#define SAT_CT_EXCL_COUNTERS 1

// What one control tick tells the tuner. Hot-water ticks and ticks while SAT is not the active controller carry no signal.
struct SatTuneTickIn {
  bool  satActive;     // SAT is the active controller
  bool  dhw;           // hot water in progress (state.sat.bDhwActive)
  bool  sunValid;      // a sun elevation is known
  float sunElevation;  // degrees
  bool  pwm;           // SAT is in PWM mode
  bool  pwmGas;        // ADR-231: PWM ticks follow the gas-boiler rule (heat source gas boiler or AUTO); set in satCurveTuneNoteTick()
  bool  chOn;          // CH enabled (CH=1 sent)
  bool  flameOn;
  bool  roomLost;      // no valid room temperature on this tick (heating on the curve only)
  bool  diagEvent;     // TASK-1882 (entry 358): a no-ignition or heating-failure event is active (state.sat)
  float relMod;        // MsgID 17 relative modulation, %
  float minMod;        // MsgID 15 low byte, %; 0 = never seen
  float signal;        // curve integral sample, C (ignored for dhw / not-active ticks)
  float error;         // PID error target - room, C (state.sat.fError, held by the 60 s PID step); NaN = no room temperature
};

// The two phases of ADR-228. Index into the per-phase arrays.
enum SatTunePhase : uint8_t { SATP_WARMUP = 0, SATP_FLOATING = 1, SATP_COUNT = 2 };

struct SatTune {
  // current hour
  uint32_t ticks;        // ticks noted this hour
  uint32_t sigTicks;     // ticks that carried a signal
  float    sigSum;       // sum of the signals
  uint32_t pwmTicks;     // ticks in SAT PWM this hour (logged for heat pumps, not used by the tuner; entry 205, TASK-1551)
  uint32_t phTicks[SATP_COUNT];   // signal ticks per phase this hour (ADR-228)
  float    phSum[SATP_COUNT];     // sum of their signals
  uint8_t  reason;       // first exclusion seen this hour, tick or whole-hour marker (SATT_NONE while nothing was excluded)
  bool     coeffChanged; // the coefficient changed during this hour
  // TASK-1932 (ADR-234): per-tick exclusions only lower the clean share; a whole-hour marker (not_sat from the service,
  // not_controlling, coeff_changed, few_ticks, no_ticks) still drops the hour. The clean sums carry only clean ticks.
  bool     hourMarked;   // a whole-hour marker was set this hour
  uint32_t exclTicks;    // ticks a per-tick exclusion applied to
  uint32_t cSigTicks;    // clean ticks that carried a signal
  float    cSigSum;
  uint32_t cPhTicks[SATP_COUNT];
  float    cPhSum[SATP_COUNT];
  // current day
  uint8_t  hoursInDay;
  uint8_t  cleanInDay;
  float    cleanSumInDay;   // sum of the signal means of the clean hours
  uint32_t phTicksInDay[SATP_COUNT];   // per-phase signal ticks of the clean hours of the day (ADR-228)
  float    phSumInDay[SATP_COUNT];
  // window of counted days
  uint8_t  winDays;
  uint8_t  winClean[SAT_CT_WINDOW_DAYS];
  float    winSum[SAT_CT_WINDOW_DAYS];
  uint32_t winPhTicks[SAT_CT_WINDOW_DAYS][SATP_COUNT];   // per counted day: per-phase ticks and sums (ADR-228)
  float    winPhSum[SAT_CT_WINDOW_DAYS][SATP_COUNT];
  // counters for the log
  uint32_t hourNo;
  uint32_t dayNo;
  // TASK-1881 telemetry: excluded hours per reason since the window last emptied (index SatTuneReason, SATT_NONE unused,
  // saturating), and the last daily record. Not in /sat_tune.json: after a reboot the counters start at 0.
  uint16_t exclHours[SATT_COUNT];
  bool        haveLast;
  uint32_t    lastDay;
  int8_t      lastDecision;   // +1 raise, -1 lower, 0 hold
  const char* lastReason;     // PSTR of the last SatTuneDayOut.reason
};

// TASK-1881: what REST and MQTT show of the tuner, copied by satCurveTuneGetProgress() on the loop task into state.sat.
struct SatTuneProgress {
  bool     running;           // the tuner clock runs (self-tune on and SAT enabled)
  uint8_t  days;              // counted decision blocks (= counted hours, ADR-260) in the window: REST/MQTT window_hours
  bool     haveLast;          // a day has closed since the tuner started
  uint32_t lastDay;
  int8_t   lastDecision;      // +1 raise, -1 lower, 0 hold
  char     lastReason[16];    // longest reason: "day_not_counted", "phases_disagree" (15)
  uint16_t exclHours[SATT_COUNT];
};

struct SatTuneHourOut {
  uint32_t hour;
  uint32_t ticks;
  bool     haveMean;
  float    mean;        // mean signal of the hour: a counted hour over its clean ticks, a not-counted hour over all ticks that carried one
  uint32_t phTicks[SATP_COUNT];   // per-phase signal ticks of the hour (ADR-228)
  bool     phHaveMean[SATP_COUNT];
  float    phMean[SATP_COUNT];
  uint32_t pwmTicks;    // ticks in SAT PWM (TASK-1551)
  bool     counted;
  uint8_t  reason;
  bool     dayComplete; // this hour was the last of the day: call satTuneCloseDay()
};

struct SatTuneDayOut {
  uint32_t    day;
  uint8_t     clean;        // clean hours of the day
  bool        counted;
  uint8_t     daysInWindow; // counted days the decision used (before a change empties the window)
  bool        haveMean3;
  float       mean3;        // all clean-hour signals, hour-weighted (ADR-205 value, logged for comparison, no longer decides)
  uint32_t    phTicks3[SATP_COUNT];    // per-phase signal ticks over the window (ADR-228)
  bool        phHaveMean3[SATP_COUNT];
  float       phMean3[SATP_COUNT];     // per-phase tick-weighted mean over the window
  int8_t      phSay[SATP_COUNT];       // per-phase verdict: +1 higher, -1 lower, 0 inside the band (or no ticks)
  bool        floatVotes;              // the floating phase had SAT_CT_FLOAT_MIN_MS of data and took part (entry 315)
  int8_t      decision;     // +1 raise, -1 lower, 0 hold
  const char* reason;       // PSTR: why hold, or what drove the change
  float       newCoeff;     // equals the old coefficient unless decision != 0
};
