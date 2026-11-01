/**
 * Host test for the latched cold cutoff with hysteresis (TASK-1547, Sergeant D entry 199).
 * Compiles the real src/OTGW-firmware/SATchEnable.h and replays the requested-setpoint sequence from
 * gruijter's alpha.597 capture (2026-11-01): the request hovers on the 28.2 C radiator cold setpoint
 * (28.1 / 28.8 / 26.5 / 28.8 ...), which made the bare rule flip CH every tick (60 s burns).
 * The bare rule (satColdCutoff, before TASK-1547) is run beside the latched rule; the test fails when
 * the latched rule flips more than the expected number of times, or when it gets a plain case wrong.
 *
 * Build & run (from repo root):
 *   g++ -std=c++17 tests/test_sat_cold_hysteresis.cpp -o tests/test_sat_cold_hysteresis.out
 *   ./tests/test_sat_cold_hysteresis.out
 */
#include <cstdio>
#include "../src/OTGW-firmware/SATchEnable.h"

static int failures = 0;

static void check(const char* name, bool ok) {
  printf("%-60s %s\n", name, ok ? "PASS" : "FAIL");
  if (!ok) failures++;
}

// Runs a sequence through the latched rule and the CH decision (continuous mode: drives=false),
// returns the number of CH changes; also counts the bare rule's changes for comparison.
static int countFlips(const float* seq, int n, float cold, float band, int* bareFlips) {
  bool cut = false;
  int flips = 0, bare = 0;
  int prevCh = -1, prevBare = -1;
  for (int i = 0; i < n; i++) {
    cut = satColdCutoffLatched(seq[i], cold, cut, band);
    const float threshold = cold + (cut ? band : 0.0f);
    const int ch = satChEnableDecide(seq[i], threshold, false, false);
    // The CH bit computed from the threshold must agree with the latch on every tick.
    if ((ch == 0) != cut) { printf("  tick %d: CH %d disagrees with latch %d\n", i, ch, cut); failures++; }
    const int chBare = satColdCutoff(seq[i], cold) ? 0 : 1;
    if (prevCh >= 0 && ch != prevCh) flips++;
    if (prevBare >= 0 && chBare != prevBare) bare++;
    prevCh = ch; prevBare = chBare;
  }
  *bareFlips = bare;
  return flips;
}

int main() {
  printf("=== satColdCutoffLatched, plain cases (radiators 28.2, band 0.5) ===\n");
  check("heating, req 28.3 -> stays on",            !satColdCutoffLatched(28.3f, 28.2f, false, 0.5f));
  check("heating, req 28.2 -> cut (at or below)",    satColdCutoffLatched(28.2f, 28.2f, false, 0.5f));
  check("cut, req 28.6 -> stays cut",                satColdCutoffLatched(28.6f, 28.2f, true, 0.5f));
  check("cut, req 28.7 -> stays cut (at cold+band)", satColdCutoffLatched(28.7f, 28.2f, true, 0.5f));
  check("cut, req 28.8 -> heats again",             !satColdCutoffLatched(28.8f, 28.2f, true, 0.5f));
  check("underfloor heating, req 21.0 -> cut",       satColdCutoffLatched(21.0f, 21.0f, false, 0.5f));
  check("underfloor cut, req 21.4 -> stays cut",     satColdCutoffLatched(21.4f, 21.0f, true, 0.5f));
  check("underfloor cut, req 21.6 -> heats again",  !satColdCutoffLatched(21.6f, 21.0f, true, 0.5f));
  check("band 0 equals the bare rule (28.3, cut)",  !satColdCutoffLatched(28.3f, 28.2f, true, 0.0f));

  printf("=== gruijter 2026-11-01 sequence, 30 s ticks ===\n");
  // 12:27..12:29 from the capture narration (28.1 off, 28.8 on, 26.5 off), extended with the afternoon
  // pattern: request 28.0-28.6 around the 28.2 C edge for 20 ticks, then a real rise to 29.5.
  const float seq[] = {28.1f, 28.8f, 26.5f, 28.4f, 28.1f, 28.3f, 28.0f, 28.5f, 28.2f, 28.6f,
                       28.1f, 28.4f, 28.0f, 28.3f, 28.6f, 28.2f, 28.5f, 28.1f, 28.4f, 28.3f,
                       29.0f, 29.5f, 29.5f, 29.2f};
  int bare = 0;
  const int flips = countFlips(seq, sizeof(seq) / sizeof(seq[0]), 28.2f, 0.5f, &bare);
  printf("  bare rule CH changes: %d   latched rule CH changes: %d\n", bare, flips);
  check("bare rule flips on this sequence (shows the defect)", bare >= 10);
  // Latched: off at 28.1, on at 28.8 (above 28.7), off at 26.5, stays off through 28.0-28.6, on at 29.0.
  check("latched rule: 3 changes only (off->on->off->on)", flips == 3);

  printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "OK", failures);
  return failures ? 1 : 0;
}
