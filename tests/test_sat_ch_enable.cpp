/**
 * Host test for the SAT CH enable rule (TASK-1316, simplified by TASK-1323).
 * Compiles the real src/OTGW-firmware/SATchEnable.h. Rule (Sergeant D, 2026-10-23):
 * requested setpoint at or below the per-system cold setpoint (28.2 radiators, 21.0 underfloor)
 * -> CS=10 + CH=0; PWM OFF phase driving the CS -> CS=10 + CH=0; everything else CH=1.
 * The rule of TASK-1316 (CH from the sent CS against the cold setpoint, PWM ON forced to 1) is
 * printed as "old" to show which cases changed.
 *
 * Build & run (from repo root):
 *   g++ -std=c++17 tests/test_sat_ch_enable.cpp -o tests/test_sat_ch_enable.out
 *   ./tests/test_sat_ch_enable.out
 */
#include <cmath>
#include <cstdio>
#include "../src/OTGW-firmware/SATchEnable.h"

static int failures = 0;

// TASK-1316 rule: decided on the sent CS (here the held/pre-solar setpoint) against the cold setpoint,
// cutoff only when requested < cold (strict), PWM ON forced to 1, PWM OFF 0.
static uint8_t oldRule(float requested, float sentCs, float cold, bool drives, bool on) {
  if (requested < cold) return 0;
  if (drives) return on ? 1 : 0;
  return (lroundf(sentCs * 10.0f) >= lroundf(cold * 10.0f)) ? 1 : 0;
}

struct D { const char* name; float requested, cold; bool drives, on; float sentCs; uint8_t want; };

int main() {
  printf("=== satChEnableDecide (new) vs TASK-1316 rule (old) ===\n");
  const D d[] = {
    // case 1: protection off, PWM keeps the held CS 24, requested 30 on radiators -> CH=1
    {"case1 held CS 24, req 30 rad",   30.0f, 28.2f, false, false, 24.0f, 1},
    {"requested exactly 28.2 rad",     28.2f, 28.2f, false, false, 28.2f, 0},   // thermo-nova <=
    {"requested 28.3 rad",             28.3f, 28.2f, false, false, 28.3f, 1},
    {"PWM OFF driving, req 35",        35.0f, 28.2f, true,  false, 10.0f, 0},
    {"PWM ON driving, req 35",         35.0f, 28.2f, true,  true,  23.0f, 1},   // low flame-keeping CS, CH stays 1
    {"continuous req 30 rad",          30.0f, 28.2f, false, false, 30.0f, 1},
    {"underfloor req 21.0",            21.0f, 21.0f, false, false, 21.0f, 0},   // <=
    {"underfloor req 21.1",            21.1f, 21.0f, false, false, 21.1f, 1},
    {"underfloor req 20.9",            20.9f, 21.0f, false, false, 21.0f, 0},
    {"PWM ON driving, req below cold", 25.0f, 28.2f, true,  true,  28.2f, 0},   // cutoff wins over PWM ON
    {"solar: req 30 - 2 = 28 rad",     28.0f, 28.2f, false, false, 28.2f, 0},   // solar lowers the requested setpoint
    {"solar: req 32 - 2 = 30 rad",     30.0f, 28.2f, false, false, 30.0f, 1},
    {"continuous req 45 rad",          45.0f, 28.2f, false, false, 45.0f, 1},
  };
  for (unsigned i = 0; i < sizeof(d) / sizeof(d[0]); i++) {
    const uint8_t got = satChEnableDecide(d[i].requested, d[i].cold, d[i].drives, d[i].on);
    const uint8_t old = oldRule(d[i].requested, d[i].sentCs, d[i].cold, d[i].drives, d[i].on);
    const bool ok = (got == d[i].want);
    printf("%-32s req %5.2f  new CH=%u  old CH=%u  want %u  %s\n", d[i].name, d[i].requested, got, old, d[i].want, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
  }
  printf("=== satColdCutoff ===\n");
  const struct { float req, cold; bool want; } c[] = {{28.2f, 28.2f, true}, {28.19f, 28.2f, true}, {28.21f, 28.2f, false}, {21.0f, 21.0f, true}, {21.1f, 21.0f, false}};
  for (unsigned i = 0; i < sizeof(c) / sizeof(c[0]); i++) {
    const bool got = satColdCutoff(c[i].req, c[i].cold);
    printf("cutoff(%5.2f, %4.1f) = %d  want %d  %s\n", c[i].req, c[i].cold, got, c[i].want, got == c[i].want ? "PASS" : "FAIL");
    if (got != c[i].want) failures++;
  }
  printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}
