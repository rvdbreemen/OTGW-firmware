// SATcurveTune.h - TASK-1525 (ADR-205) heating curve self-tune: constants and types.
// Kept in a header, included from SATtypes.h, because the Arduino builder hoists function prototypes above
// any type defined later in a .ino (the functions live in SATcontrol.ino, satCurveTune blocks).
// tests/test_sat_curve_selftune.py compiles this header together with those blocks.
#pragma once
#include <stdint.h>

#define SAT_CT_WINDOW_DAYS      3      // counted days the decision looks at (entries 172, 175)
#define SAT_CT_MIN_CLEAN_HOURS  6      // clean hours a day needs to count (entry 164)
#define SAT_CT_HOURS_PER_DAY    24
#define SAT_CT_SUN_MAX_DEG      12.0f  // clean hours need the sun below this (entry 161)
#define SAT_CT_BAND_C           1.0f   // decision threshold, symmetric (entry 165)
#define SAT_CT_STEP             0.1f   // fixed step (entry 165)
#define SAT_CT_COEFF_MIN        0.1f   // coefficient setting range (settingStuff.ino, ADR-198)
#define SAT_CT_COEFF_MAX        5.0f
#define SAT_CT_DEFAULT_MIN_MOD  5.0f   // minimum modulation when MsgID 15 was never seen (entry 174)
#define SAT_CT_MIN_TICK_FRACTION  0.75f  // an hour counts only with this share of its expected control ticks (hour length / control interval)
// Hours with a lost room temperature: the signal is then 0 (curve only), which dilutes the mean. 0 = such hours still count,
// 1 = they are excluded with reason no_room. Pending Sergeant D (Q-1525a); a bench or test build may define it as 1.
#ifndef SAT_CT_EXCLUDE_ROOM_LOST
#define SAT_CT_EXCLUDE_ROOM_LOST  0
#endif

enum SatTuneReason : uint8_t {
  SATT_NONE = 0, SATT_NOT_SAT, SATT_DHW, SATT_NO_SUN, SATT_SUN, SATT_PWM, SATT_FLAME_OFF, SATT_MIN_MOD,
  SATT_COEFF_CHANGED, SATT_NO_TICKS, SATT_NOT_CONTROLLING, SATT_FEW_TICKS, SATT_NO_ROOM
};

// What one control tick tells the tuner. Hot-water ticks and ticks while SAT is not the active controller carry no signal.
struct SatTuneTickIn {
  bool  satActive;     // SAT is the active controller
  bool  dhw;           // hot water in progress (state.sat.bDhwActive)
  bool  sunValid;      // a sun elevation is known
  float sunElevation;  // degrees
  bool  pwm;           // SAT is in PWM mode
  bool  chOn;          // CH enabled (CH=1 sent)
  bool  flameOn;
  bool  roomLost;      // no valid room temperature on this tick (heating on the curve only)
  float relMod;        // MsgID 17 relative modulation, %
  float minMod;        // MsgID 15 low byte, %; 0 = never seen
  float signal;        // curve integral sample, C (ignored for dhw / not-active ticks)
};

struct SatTune {
  // current hour
  uint32_t ticks;        // ticks noted this hour
  uint32_t sigTicks;     // ticks that carried a signal
  float    sigSum;       // sum of the signals
  uint8_t  reason;       // first exclusion seen this hour (SATT_NONE while every tick was clean)
  bool     coeffChanged; // the coefficient changed during this hour
  // current day
  uint8_t  hoursInDay;
  uint8_t  cleanInDay;
  float    cleanSumInDay;   // sum of the signal means of the clean hours
  // window of counted days
  uint8_t  winDays;
  uint8_t  winClean[SAT_CT_WINDOW_DAYS];
  float    winSum[SAT_CT_WINDOW_DAYS];
  // counters for the log
  uint32_t hourNo;
  uint32_t dayNo;
};

struct SatTuneHourOut {
  uint32_t hour;
  uint32_t ticks;
  bool     haveMean;
  float    mean;        // mean signal of the hour (all ticks that carried one)
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
  float       mean3;
  int8_t      decision;     // +1 raise, -1 lower, 0 hold
  const char* reason;       // PSTR: why hold, or what drove the change
  float       newCoeff;     // equals the old coefficient unless decision != 0
};
