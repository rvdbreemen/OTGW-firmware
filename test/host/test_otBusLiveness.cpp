//=======================================================================
// test_otBusLiveness.cpp
//
// Unit tests for the OT-bus presence decision behind TASK-1135, compiled
// against the REAL src/OTGW-firmware/otBusLiveness.h. Nothing in that header is
// reimplemented here; the clock is simply passed in, which is the whole reason
// the decision was separated from the publishing.
//
// The defect this guards: the decision used to live inside processOT(), so it
// was only evaluated when a frame arrived. A bus that went completely silent
// therefore never had its timeout evaluated, and the presence flags held their
// last value until reboot. These tests pin the behaviour that a driven clock
// alone is enough to flip the verdict.
//=======================================================================

#include "../../src/OTGW-firmware/otBusLiveness.h"

#include <cstdio>

static int g_checks = 0;
static int g_failures = 0;

static void check(bool condition, const char* what) {
  g_checks++;
  if (condition) {
    std::printf("  [ OK ] %s\n", what);
  } else {
    g_failures++;
    std::printf("  [FAIL] %s\n", what);
  }
}

// A plausible epoch, so "never seen" (0) is genuinely in the past and the
// arithmetic is nowhere near a boundary of time_t.
static const time_t T0 = 1789000000;

int main() {
  std::printf("otBusLiveness host tests\n");

  //-------------------------------------------------------------------
  // (a) The defect: silence alone must flip the verdict.
  //-------------------------------------------------------------------
  std::printf("\n(a) silence flips the verdict without any new frame\n");
  {
    // Both sides heard just now, previously published as present.
    OtBusLiveness v = evalOtBusLiveness(T0, T0, T0, true, true, true, false, false);
    check(v.bBoiler && v.bThermostat && v.bOnline, "both sides present right after being heard");
    check(!v.bBoilerChanged && !v.bThermostatChanged && !v.bOnlineChanged,
          "nothing to publish while the verdict is unchanged");

    // No new frames. Only the clock moves past the window.
    v = evalOtBusLiveness(T0 + OTBUS_PRESENCE_TIMEOUT_SEC, T0, T0, true, true, true, false, false);
    check(!v.bBoiler && !v.bThermostat, "both sides absent once the window has passed");
    check(!v.bOnline, "the bus is offline when neither side is present");
    check(v.bBoilerChanged && v.bThermostatChanged && v.bOnlineChanged,
          "all three transitions are flagged for publishing");
  }

  //-------------------------------------------------------------------
  // (b) The boundary. `now < lastSeen + TIMEOUT`, so exactly TIMEOUT is gone.
  //-------------------------------------------------------------------
  std::printf("\n(b) timeout boundary\n");
  {
    OtBusLiveness v = evalOtBusLiveness(T0 + OTBUS_PRESENCE_TIMEOUT_SEC - 1, T0, T0,
                                        true, true, true, false, false);
    check(v.bBoiler, "one second inside the window still counts as present");

    v = evalOtBusLiveness(T0 + OTBUS_PRESENCE_TIMEOUT_SEC, T0, T0, true, true, true, false, false);
    check(!v.bBoiler, "exactly at the window counts as gone");
  }

  //-------------------------------------------------------------------
  // (c) The two sides are independent, and bOnline is their OR.
  //-------------------------------------------------------------------
  std::printf("\n(c) the sides are independent\n");
  {
    // Thermostat heard now, boiler long gone.
    OtBusLiveness v = evalOtBusLiveness(T0 + 100, T0, T0 + 100, true, true, true, false, false);
    check(!v.bBoiler, "boiler gone");
    check(v.bThermostat, "thermostat still present");
    check(v.bOnline, "the bus is online while either side talks");
    check(v.bBoilerChanged, "the boiler transition is flagged");
    check(!v.bThermostatChanged, "the thermostat is unchanged, so not flagged");
    check(!v.bOnlineChanged, "the bus stayed online, so not flagged");
  }

  //-------------------------------------------------------------------
  // (d) A gateway that has never heard anything reads as absent, and says so
  //     on the first evaluation rather than sitting silent.
  //-------------------------------------------------------------------
  std::printf("\n(d) never heard anything\n");
  {
    OtBusLiveness v = evalOtBusLiveness(T0, 0, 0, false, false, false, false, false);
    check(!v.bBoiler && !v.bThermostat && !v.bOnline, "a bus that was never heard is absent");
    check(!v.bBoilerChanged, "no transition when absent was already the published value");

    // forcePublish is what the first frame after boot uses: publish the verdict
    // even though it did not change.
    v = evalOtBusLiveness(T0, 0, 0, false, false, false, true, false);
    check(v.bBoilerChanged && v.bThermostatChanged && v.bOnlineChanged,
          "forcePublish flags all three without a change");
  }

  //-------------------------------------------------------------------
  // (e) Flash suppression. A PIC update stops the stream for longer than the
  //     window, so the verdict must freeze instead of flapping.
  //-------------------------------------------------------------------
  std::printf("\n(e) frozen while flashing\n");
  {
    // Far past the window, which without suppression would read as absent.
    const time_t late = T0 + 10 * OTBUS_PRESENCE_TIMEOUT_SEC;

    OtBusLiveness v = evalOtBusLiveness(late, T0, T0, true, true, true, false, true);
    check(v.bBoiler && v.bThermostat && v.bOnline, "the previous verdict is kept while flashing");
    check(!v.bBoilerChanged && !v.bThermostatChanged && !v.bOnlineChanged,
          "nothing is published while flashing");

    // forcePublish must not punch through the freeze either: a first frame
    // arriving mid-flash is not a reason to announce a stale verdict.
    v = evalOtBusLiveness(late, T0, T0, true, true, true, true, true);
    check(!v.bBoilerChanged && !v.bThermostatChanged && !v.bOnlineChanged,
          "forcePublish does not override the freeze");

    // And the freeze preserves absence just as faithfully as presence.
    v = evalOtBusLiveness(T0, T0, T0, false, false, false, false, true);
    check(!v.bBoiler && !v.bThermostat && !v.bOnline,
          "a previously-absent verdict is not revived by the freeze");

    // Once the flash ends, the very next evaluation tells the truth.
    v = evalOtBusLiveness(late, T0, T0, true, true, true, false, false);
    check(!v.bBoiler && v.bBoilerChanged, "the verdict catches up as soon as the flash ends");
  }

  //-------------------------------------------------------------------
  // (f) Regression shape: a long run of evaluations with no new frames must
  //     converge to absent and then stay quiet, not re-announce every tick.
  //-------------------------------------------------------------------
  std::printf("\n(f) a silent bus converges and then stops publishing\n");
  {
    bool prevB = true, prevT = true, prevO = true;
    int publishes = 0;
    for (int tick = 0; tick < 100; tick++) {
      // Every 3 seconds, which is the tick the firmware uses.
      OtBusLiveness v = evalOtBusLiveness(T0 + tick * 3, T0, T0,
                                          prevB, prevT, prevO, false, false);
      if (v.bBoilerChanged) { publishes++; prevB = v.bBoiler; }
      if (v.bThermostatChanged) { prevT = v.bThermostat; }
      if (v.bOnlineChanged) { prevO = v.bOnline; }
    }
    check(!prevB && !prevT && !prevO, "converged to absent");
    check(publishes == 1, "boiler_connected was published exactly once, not on every tick");
  }

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
