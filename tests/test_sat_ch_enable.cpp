/**
 * Host test for the SAT CH enable rule (TASK-1316).
 * Compiles the real src/OTGW-firmware/SATchEnable.h: CH is on at or above the per-system
 * cold setpoint (28.2 radiators, 21.0 underfloor), quantized to 0.1 C like CS=%.1f.
 * The old rule (setpoint > SAT_MIN_SETPOINT 10) is printed next to it to show the red cases.
 *
 * Build & run (from repo root):
 *   g++ -std=c++17 tests/test_sat_ch_enable.cpp -o tests/test_sat_ch_enable.out
 *   ./tests/test_sat_ch_enable.out
 */
#include <cstdio>
#include "../src/OTGW-firmware/SATchEnable.h"

static int failures = 0;
static uint8_t oldRule(float sp) { return sp > 10.0f ? 1 : 0; }

static void check(const char* sys, float cold, float sp, uint8_t want) {
  const uint8_t got = satChEnableFor(sp, cold);
  const bool ok = (got == want);
  printf("%-10s cold %.1f  setpoint %5.2f  new CH=%u  old CH=%u  want %u  %s\n",
         sys, cold, sp, got, oldRule(sp), want, ok ? "PASS" : "FAIL");
  if (!ok) failures++;
}

int main() {
  printf("=== SAT CH enable rule ===\n");
  const float rad[] = {10, 22, 25, 28.1f, 28.15f, 28.17f, 28.2f, 28.25f, 45};
  const uint8_t radWant[] = {0, 0, 0, 0, 1, 1, 1, 1, 1};
  for (unsigned i = 0; i < sizeof(rad) / sizeof(rad[0]); i++) check("radiators", 28.2f, rad[i], radWant[i]);
  const float fl[] = {20.9f, 21.0f, 25};
  const uint8_t flWant[] = {0, 1, 1};
  for (unsigned i = 0; i < sizeof(fl) / sizeof(fl[0]); i++) check("underfloor", 21.0f, fl[i], flWant[i]);
  printf("=== satChEnableDecide ===\n");
  struct D { const char* name; float pre, cold; bool drives, on, cutoff; uint8_t want; };
  const D d[] = {
    {"PWM ON cs 25 rad",            25.0f, 28.2f, true,  true,  false, 1},
    {"PWM OFF cs 10",               10.0f, 28.2f, true,  false, false, 0},
    {"continuous 25 rad",           25.0f, 28.2f, false, false, false, 0},
    {"continuous 28.2 rad",         28.2f, 28.2f, false, false, false, 1},
    {"continuous 45 rad",           45.0f, 28.2f, false, false, false, 1},
    {"underfloor continuous 21",    21.0f, 21.0f, false, false, false, 1},
    {"underfloor continuous 20.9",  20.9f, 21.0f, false, false, false, 0},
    {"solar: pre 28.2 (sent 26.2)", 28.2f, 28.2f, false, false, false, 1},
    {"cold cutoff + pwmOn",         25.0f, 28.2f, true,  true,  true,  0},
    {"pwm not driving, on, pre 25", 25.0f, 28.2f, false, true,  false, 0},
  };
  for (unsigned i = 0; i < sizeof(d) / sizeof(d[0]); i++) {
    const uint8_t got = satChEnableDecide(d[i].pre, d[i].cold, d[i].drives, d[i].on, d[i].cutoff);
    const bool ok = (got == d[i].want);
    printf("%-30s pre %5.2f  new CH=%u  old CH=%u  want %u  %s\n", d[i].name, d[i].pre, got, oldRule(d[i].pre), d[i].want, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
  }
  printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}
