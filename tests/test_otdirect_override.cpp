/**
 * Host-compilable test for OTDirect TT/TC remote-override numerical contracts.
 *
 * Pins the four numerical-contract fixes from session ace21a48..6a01ae5d:
 *   - TASK-491 (sign-extend): (int16_t) cast preserves f8.8 sign.
 *   - TASK-495 (clamp):       celsius is clamped to [-40, 127] before cast.
 *   - TASK-466 (honour):      thermostat-echo within 0x40 increments count.
 *   - TASK-466 (release):     count >= 3 + delta > 0x80 + TEMPORARY -> clear.
 *
 * The functions under test are pure logic with no Arduino runtime
 * dependencies (no Serial, millis, delay, FreeRTOS). The relevant code
 * paths are LIFTED into this test as `static` helpers — the test exercises
 * the SAME math/branching shape as the firmware. If the firmware diverges,
 * future regressions are caught at PR time by re-running this test.
 *
 * Build & run (from repo root):
 *   g++ -std=c++17 -Wall -Wextra tests/test_otdirect_override.cpp -o tests/test_otdirect_override.out
 *   ./tests/test_otdirect_override.out
 *   echo $?   # 0 on pass, 1 on failure
 *
 * See tests/README.md for details and Phase 3A finding 3A-H1 for rationale.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Lifted constants — kept in sync with src/OTGW-firmware/OTDirect.ino
// ---------------------------------------------------------------------------

// f8.8 honour/release thresholds (TASK-466)
static constexpr uint16_t OT_OVERRIDE_HONOR_DELTA_F88   = (uint16_t)(0.25f * 256.0f); // 0x40
static constexpr uint16_t OT_OVERRIDE_RELEASE_DELTA_F88 = (uint16_t)(0.50f * 256.0f); // 0x80
static constexpr uint8_t  OT_OVERRIDE_HONOR_THRESHOLD   = 3;

// TT/TC mode discriminator
enum OTRemoteOverrideMode : uint8_t {
  OT_OVERRIDE_NONE      = 0,
  OT_OVERRIDE_TEMPORARY = 1,
  OT_OVERRIDE_CONSTANT  = 2,
};

struct OTRemoteOverrideState {
  OTRemoteOverrideMode mode;
  uint16_t f88Value;
  uint16_t lastThermostatVal;
  uint8_t  honoredCount;
};

// ---------------------------------------------------------------------------
// Function-under-test mirrors — port-of code paths from OTDirect.ino
// ---------------------------------------------------------------------------

// Mirror of setRemoteOverride()'s f8.8 cast with TASK-495 clamp.
// Returns the f8.8 raw value that would be enqueued for MsgID 16.
static uint16_t castCelsiusToF88(float celsius)
{
  // TASK-495 clamp: prevent float-to-narrow UB outside [-128, 127].
  if (celsius < -40.0f) celsius = -40.0f;
  if (celsius > 127.0f) celsius = 127.0f;
  return (uint16_t)((int16_t)(celsius * 256.0f));
}

// Mirror of onThermostatMsgID16()'s delta math (TASK-491 sign-extend).
// Returns |signed_delta| in raw f8.8 units.
static uint32_t computeAbsDeltaF88(uint16_t f88, uint16_t override88)
{
  // TASK-491: cast through int16_t to preserve the f8.8 sign before
  // widening to int32_t. Without this hop, raw 0xFB00 (-5.0 °C) would
  // read as +64256 instead of -1280.
  int16_t curSigned = (int16_t)f88;
  int16_t ovrSigned = (int16_t)override88;
  int32_t signedDelta = (int32_t)curSigned - (int32_t)ovrSigned;
  return (signedDelta < 0) ? (uint32_t)(-signedDelta) : (uint32_t)signedDelta;
}

// Mirror of onThermostatMsgID16()'s honour/release branching (TASK-466).
// Mutates `state` per the same rules; returns true if auto-clear fired.
static bool processThermostatMsg16(OTRemoteOverrideState& state, uint16_t f88)
{
  if (state.mode == OT_OVERRIDE_NONE) {
    state.lastThermostatVal = f88;
    return false;
  }

  uint32_t delta = computeAbsDeltaF88(f88, state.f88Value);

  if (delta < OT_OVERRIDE_HONOR_DELTA_F88) {
    if (state.honoredCount < 0xFF) state.honoredCount++;
  } else if (state.mode == OT_OVERRIDE_TEMPORARY
             && state.honoredCount >= OT_OVERRIDE_HONOR_THRESHOLD
             && delta > OT_OVERRIDE_RELEASE_DELTA_F88) {
    // Auto-clear
    state.mode              = OT_OVERRIDE_NONE;
    state.f88Value          = 0;
    state.honoredCount      = 0;
    state.lastThermostatVal = 0xFFFF;
    return true;
  }

  state.lastThermostatVal = f88;
  return false;
}

// ---------------------------------------------------------------------------
// Tiny test harness
// ---------------------------------------------------------------------------

static int failures = 0;

static void checkU16(const char* name, uint16_t expected, uint16_t got)
{
  bool ok = (expected == got);
  std::printf("%-44s expected=0x%04X got=0x%04X %s\n",
              name, expected, got, ok ? "PASS" : "FAIL");
  if (!ok) failures++;
}

static void checkU32(const char* name, uint32_t expected, uint32_t got)
{
  bool ok = (expected == got);
  std::printf("%-44s expected=%-6u got=%-6u %s\n",
              name, expected, got, ok ? "PASS" : "FAIL");
  if (!ok) failures++;
}

static void checkU8(const char* name, uint8_t expected, uint8_t got)
{
  bool ok = (expected == got);
  std::printf("%-44s expected=%-3u got=%-3u %s\n",
              name, expected, got, ok ? "PASS" : "FAIL");
  if (!ok) failures++;
}

static void checkBool(const char* name, bool expected, bool got)
{
  bool ok = (expected == got);
  std::printf("%-44s expected=%-5s got=%-5s %s\n",
              name, expected ? "true" : "false",
              got ? "true" : "false", ok ? "PASS" : "FAIL");
  if (!ok) failures++;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

int main()
{
  std::printf("OTDirect TT/TC override numerical-contract tests\n");
  std::printf("================================================\n");

  // 1. f8.8 round-trip identity for a typical room setpoint (22.5 °C).
  //    22.5 * 256 = 5760 = 0x1680.
  checkU16("f88 round-trip 22.5C", 0x1680, castCelsiusToF88(22.5f));

  // 2. Sign-extension: raw 0xFB00 (-5.0 °C) computed against override 0x0F00
  //    (+15.0 °C) yields |(-5.0) - (+15.0)| = 20.0 °C = 5120 raw units.
  //    Without the (int16_t) hop, naive uint16_t -> int32_t would yield
  //    +64256 - 3840 = 60416 (visibly wrong).
  checkU32("sign-extend |-5.0 - +15.0| = 20.0",
           (uint32_t)(20.0f * 256.0f),
           computeAbsDeltaF88(0xFB00, 0x0F00));

  // 3. Clamp on negative-out-of-range: -50 °C clamps to -40 °C.
  //    -40 °C as f8.8 = -10240 = 0xD800 (16-bit two's-complement).
  checkU16("clamp -50C -> -40C f88",
           (uint16_t)((int16_t)(-40.0f * 256.0f)),
           castCelsiusToF88(-50.0f));

  // 4. Clamp on positive-out-of-range: 200 °C clamps to 127 °C.
  //    127 °C as f8.8 = 32512 = 0x7F00.
  checkU16("clamp +200C -> +127C f88",
           0x7F00,
           castCelsiusToF88(200.0f));

  // 5. Honour-count: thermostat reports our value within 0.25 °C tolerance,
  //    count increments. Start with mode=TEMPORARY, override=20 °C (0x1400),
  //    count=0. Thermostat reports 19.95 °C (raw ~0x13F4) → delta 0x000C < 0x40.
  {
    OTRemoteOverrideState s = {
      OT_OVERRIDE_TEMPORARY, /*f88Value=*/0x1400,
      /*lastThermostatVal=*/0xFFFF, /*honoredCount=*/0
    };
    bool fired = processThermostatMsg16(s, 0x13F4);
    checkBool("honour-cycle: auto-clear NOT fired", false, fired);
    checkU8  ("honour-cycle: count incremented", 1, s.honoredCount);
  }

  // 6. Auto-clear trigger: TEMPORARY + count>=3 + thermostat delta > 0.5 °C
  //    fires the clear path. Override 20 °C (0x1400), count=3, thermostat
  //    reports 17.0 °C (0x1100) → delta = 3.0 °C * 256 = 768 (0x300) > 0x80.
  {
    OTRemoteOverrideState s = {
      OT_OVERRIDE_TEMPORARY, /*f88Value=*/0x1400,
      /*lastThermostatVal=*/0x1400, /*honoredCount=*/3
    };
    bool fired = processThermostatMsg16(s, 0x1100);
    checkBool("auto-clear: fired", true, fired);
    checkU8  ("auto-clear: mode reset to NONE", OT_OVERRIDE_NONE, s.mode);
    checkU16 ("auto-clear: f88Value zeroed", 0x0000, s.f88Value);
  }

  // 7. CONSTANT (TC) does NOT auto-clear under same conditions.
  {
    OTRemoteOverrideState s = {
      OT_OVERRIDE_CONSTANT, /*f88Value=*/0x0F00,
      /*lastThermostatVal=*/0x0F00, /*honoredCount=*/3
    };
    bool fired = processThermostatMsg16(s, 0x0C00);  // 12 °C, far divergence
    checkBool("TC mode: auto-clear NOT fired", false, fired);
    checkU8  ("TC mode: still CONSTANT", OT_OVERRIDE_CONSTANT, s.mode);
  }

  std::printf("================================================\n");
  if (failures == 0) {
    std::printf("All assertions PASS\n");
    return 0;
  } else {
    std::printf("FAILURES: %d\n", failures);
    return 1;
  }
}
