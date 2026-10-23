/**
 * Host test for the return water refresh helpers (TASK-1314).
 * Compiles the real src/OTGW-firmware/PICreturnSensor.h: PIC version compare,
 * the "PR: E=" value parser and the PR=E send gate.
 *
 * Build & run (from repo root):
 *   g++ -std=c++17 tests/test_pic_return_sensor.cpp -o tests/test_pic_return_sensor.out
 *   ./tests/test_pic_return_sensor.out
 */
#include <cstdio>
#include "../src/OTGW-firmware/PICreturnSensor.h"

static int failures = 0;
#define CHECK(cond, name) do { bool ok_ = (cond); printf("%-52s %s\n", name, ok_ ? "PASS" : "FAIL"); if (!ok_) failures++; } while (0)

static bool ver(const char* v) { return picVersionAtLeast(v, 6, 6); }

static PicSensorEResult parse(const char* v, float& f) { f = -999.0f; return picParseSensorE(v, f); }

int main() {
  printf("=== PIC return sensor helpers ===\n");
  CHECK(!ver("6.5"),            "version 6.5 is below 6.6");
  CHECK(ver("6.6"),             "version 6.6 qualifies");
  CHECK(ver("6.8"),             "version 6.8 qualifies");
  CHECK(ver("6.10"),            "version 6.10 is newer than 6.6 (numeric)");
  CHECK(ver("7.0"),             "version 7.0 qualifies");
  CHECK(ver("6.8.1"),           "version 6.8.1 qualifies");
  CHECK(!ver("5.9"),            "version 5.9 is below 6.6");
  CHECK(!ver(""),               "empty version is false");
  CHECK(!ver(nullptr),          "null version is false");
  CHECK(!ver("garbage"),        "garbage version is false");
  CHECK(!ver("no pic found"),   "default 'no pic found' is false");
  CHECK(!ver("6"),              "version without minor is false");
  CHECK(!ver("6."),             "version with empty minor is false");
  CHECK(!ver("6.x"),            "version with non-numeric minor is false");

  float f;
  CHECK(parse("34.56", f) == PIC_SENSOR_E_VALID && f > 34.559f && f < 34.561f, "E=34.56 is valid 34.56");
  CHECK(parse("-5.25", f) == PIC_SENSOR_E_VALID && f < -5.249f && f > -5.251f,  "E=-5.25 is valid (negative)");
  CHECK(parse("-", f) == PIC_SENSOR_E_NO_READING && f == -999.0f,               "E=- is no reading, output untouched");
  CHECK(parse("", f) == PIC_SENSOR_E_INVALID && f == -999.0f,                   "E= (empty) is invalid");
  CHECK(parse("abc", f) == PIC_SENSOR_E_INVALID && f == -999.0f,                "E=abc is invalid");
  CHECK(parse("34.5x", f) == PIC_SENSOR_E_INVALID && f == -999.0f,              "trailing text is invalid");
  CHECK(parse("200.0", f) == PIC_SENSOR_E_INVALID && f == -999.0f,              "200.0 is out of range");
  CHECK(parse("-50.0", f) == PIC_SENSOR_E_INVALID && f == -999.0f,              "-50.0 is out of range");
  CHECK(parse("nan", f) == PIC_SENSOR_E_INVALID && f == -999.0f,                "nan is invalid");
  CHECK(parse(nullptr, f) == PIC_SENSOR_E_INVALID,                              "null value is invalid");
  CHECK(parse("0.00", f) == PIC_SENSOR_E_VALID && f == 0.0f,                    "a real 0.00 reading stays valid");

  CHECK( picTretPollGate(true,  false, false, "6.8",  'R'), "gate: PIC, 6.8, R sends");
  CHECK(!picTretPollGate(false, false, false, "6.8",  'R'), "gate: not PIC path (OTDirect) blocks");
  CHECK(!picTretPollGate(true,  true,  false, "6.8",  'R'), "gate: flashing blocks");
  CHECK(!picTretPollGate(true,  false, true,  "6.8",  'R'), "gate: PS=1 mode blocks");
  CHECK(!picTretPollGate(true,  false, false, "6.5",  'R'), "gate: PIC 6.5 blocks");
  CHECK(!picTretPollGate(true,  false, false, "",     'R'), "gate: unknown version blocks");
  CHECK(!picTretPollGate(true,  false, false, "6.8",  'O'), "gate: sensor function O blocks");
  CHECK(!picTretPollGate(true,  false, false, "6.8",  '\0'),"gate: no cached D blocks");

  printf(failures ? "=== %d FAILURES ===\n" : "=== ALL TESTS PASSED (failures=0) ===\n", failures);
  return failures ? 1 : 0;
}
