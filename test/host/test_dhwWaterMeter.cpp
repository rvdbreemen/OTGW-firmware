//=======================================================================
// test_dhwWaterMeter.cpp — host-compiled tests for the cumulative DHW
// water meter (TASK-1091).
//
// The REAL src/OTGW-firmware/dhwWaterMeter.ino is included below; nothing
// under test is copied or reimplemented here. Only the platform is
// emulated (arduino_shim.h).
//
// Build + run: test\run_tests.bat   (exits non-zero on failure)
//=======================================================================
#include "arduino_shim.h"

// ---- code under test, verbatim ----------------------------------------
#include "../../src/OTGW-firmware/dhwWaterMeter.ino"
// -----------------------------------------------------------------------

#include <cstdio>
#include <cmath>

static int g_failures = 0;
static int g_checks   = 0;

static void check(bool ok, const char* what) {
  g_checks++;
  if (ok) {
    printf("  [ OK ] %s\n", what);
  } else {
    g_failures++;
    printf("  [FAIL] %s\n", what);
  }
}

static void checkNear(float got, float want, const char* what) {
  const bool ok = fabsf(got - want) < 0.001f;
  g_checks++;
  if (ok) {
    printf("  [ OK ] %s (%.4f L)\n", what, got);
  } else {
    g_failures++;
    printf("  [FAIL] %s: got %.4f L, want %.4f L\n", what, got, want);
  }
}

int main() {
  printf("dhwWaterMeter host tests\n");

  //--------------------------------------------------------------------
  // (a) The first reading after boot has no interval behind it, so it
  //     must add nothing however high the flow is.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  updateDHWWaterMeter(12.0f, 1000);
  checkNear(dhwWaterTotalL, 0.0f, "first sample after boot adds nothing");

  //--------------------------------------------------------------------
  // (b) Normal cadence: 6 l/min sampled every 10 s for a full minute is
  //     exactly 6 litres.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  uint32_t t = 5000;
  updateDHWWaterMeter(6.0f, t);              // seeds, adds nothing
  for (int i = 0; i < 6; i++) {
    t += 10000;
    updateDHWWaterMeter(6.0f, t);
  }
  checkNear(dhwWaterTotalL, 6.0f, "6 l/min over 60 s = 6 L");

  //--------------------------------------------------------------------
  // (c) A gap longer than the clamp is a hole in the measurement, not
  //     water. This is the case that matters: MsgID 19 is not polled, so
  //     the bus can fall silent with a high last reading.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  updateDHWWaterMeter(8.0f, 0);              // seeds
  updateDHWWaterMeter(8.0f, 120000);         // 2 minutes later
  checkNear(dhwWaterTotalL, 0.0f, "gap of 120 s adds nothing");

  //--------------------------------------------------------------------
  // (d) The clamp boundary itself: 60000 ms still counts, 60001 does not.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  updateDHWWaterMeter(10.0f, 0);
  updateDHWWaterMeter(10.0f, 60000);
  checkNear(dhwWaterTotalL, 10.0f, "exactly 60 s is still integrated");

  resetDHWWaterMeter();
  updateDHWWaterMeter(10.0f, 0);
  updateDHWWaterMeter(10.0f, 60001);
  checkNear(dhwWaterTotalL, 0.0f, "60001 ms is over the clamp");

  //--------------------------------------------------------------------
  // (e) A gap must not leave the clock behind: after the gap, the next
  //     normal interval integrates its own duration only.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  updateDHWWaterMeter(6.0f, 0);
  updateDHWWaterMeter(6.0f, 300000);         // 5 min gap, skipped
  updateDHWWaterMeter(6.0f, 330000);         // 30 s later
  checkNear(dhwWaterTotalL, 3.0f, "interval after a gap counts 30 s, not 5.5 min");

  //--------------------------------------------------------------------
  // (f) millis() wraps every ~49.7 days. Unsigned subtraction must carry
  //     the real interval across the wrap, not a 49-day gap.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  updateDHWWaterMeter(6.0f, 0xFFFFF000UL);
  updateDHWWaterMeter(6.0f, 0x00000F40UL);   // 8000 ms later, past the wrap
  checkNear(dhwWaterTotalL, 0.8f, "millis() wrap yields the real 8 s interval");

  //--------------------------------------------------------------------
  // (g) Zero flow adds nothing but still advances the clock, so an idle
  //     stretch cannot be back-charged to the next draw.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  updateDHWWaterMeter(0.0f, 0);
  updateDHWWaterMeter(0.0f, 30000);
  checkNear(dhwWaterTotalL, 0.0f, "zero flow adds nothing");
  updateDHWWaterMeter(6.0f, 60000);
  checkNear(dhwWaterTotalL, 3.0f, "draw after idle counts only its own 30 s");

  //--------------------------------------------------------------------
  // (h) A negative reading (a decode artefact) must never subtract.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  updateDHWWaterMeter(6.0f, 0);
  updateDHWWaterMeter(6.0f, 60000);          // 6 L
  updateDHWWaterMeter(-5.0f, 120000);
  checkNear(dhwWaterTotalL, 6.0f, "negative flow does not decrease the total");

  //--------------------------------------------------------------------
  // (i) Nothing is reported until a MsgID 19 frame has actually decoded.
  //     Most installations never carry that id, and a meter pinned at
  //     0.0 L would be a fiction rather than a measurement.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  check(!dhwWaterMeterHasData(), "no data before the first MsgID 19 frame");
  updateDHWWaterMeter(4.0f, 1000);
  check(dhwWaterMeterHasData(), "has data once a frame has decoded");

  //--------------------------------------------------------------------
  // (j) The total is monotonic across a long realistic run.
  //--------------------------------------------------------------------
  resetDHWWaterMeter();
  float previous = 0.0f;
  bool monotonic = true;
  t = 0;
  updateDHWWaterMeter(0.0f, t);
  for (int i = 0; i < 500; i++) {
    t += 1000;
    updateDHWWaterMeter((i % 7 == 0) ? 0.0f : 9.5f, t);
    if (dhwWaterTotalL < previous) monotonic = false;
    previous = dhwWaterTotalL;
  }
  check(monotonic, "total never decreases over a 500-sample run");

  //--------------------------------------------------------------------
  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
