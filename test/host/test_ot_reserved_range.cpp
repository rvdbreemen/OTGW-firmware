// test/host/test_ot_reserved_range.cpp  (TASK-1174)
//
// Checks which OpenTherm data-ids the firmware treats as reserved once a v4.x
// device is seen, against OT Protocol Specification v4.2: the application-layer
// table lists 48, 49, 56 (TdhwSet) and 57 (MaxTSet) and then jumps to 70, and the
// 4.x changelog removes ids 50 and 58. So under v4.x rules the reserved set must be
// exactly 50-55 and 58-69 (the 1.x fix for GitHub #538/#540), and nothing is
// reserved while only pre-4.0 versions have been seen.
//
// The functions under test are NOT copied here. test_ot_reserved_range.py slices
// the enum, the mode variable and isLegacyPreV42CompatibilityId /
// useV4xReservedIdRules / isMsgIdReservedInActiveProfile out of
// src/OTGW-firmware/OTGW-Core.ino (any git revision or the working tree) into
// generated/ot_reserved_range.inc, which is included below.

#include <cstdint>
#include <cstdio>

// Minimal stand-ins for what the sliced code reads.
#define PROGMEM
struct OTSystemStateStub {
  float OpenThermVersionSlave;
  float OpenThermVersionMaster;
};
static OTSystemStateStub OTcurrentSystemState = {0.0f, 0.0f};

#include "generated/ot_reserved_range.inc"

static bool specReservedV4x(int id) {
  return (id >= 50 && id <= 55) || (id >= 58 && id <= 69);
}

static int checkVersion(float slave, float master, bool v4x) {
  OTcurrentSystemState.OpenThermVersionSlave = slave;
  OTcurrentSystemState.OpenThermVersionMaster = master;
  int failures = 0;
  printf("versions slave=%.2f master=%.2f -> reserved:", slave, master);
  for (int id = 0; id <= 255; id++) {
    bool got = isMsgIdReservedInActiveProfile((uint8_t)id);
    bool want = v4x && specReservedV4x(id);
    if (got) printf(" %d", id);
    if (got != want) failures++;
  }
  printf("\n");
  if (failures) {
    printf("  MISMATCH vs spec for:");
    for (int id = 0; id <= 255; id++) {
      bool got = isMsgIdReservedInActiveProfile((uint8_t)id);
      bool want = v4x && specReservedV4x(id);
      if (got != want) printf(" %d(%s)", id, got ? "reserved" : "free");
    }
    printf("\n");
  }
  return failures;
}

int main() {
  int failures = 0;
  failures += checkVersion(3.00f, 0.00f, false);  // pre-4.0 slave only
  failures += checkVersion(4.00f, 0.00f, true);   // slave reports 4.00 (fixture B407D0400)
  failures += checkVersion(0.00f, 4.20f, true);   // master reports 4.2
  bool r56 = false, r57 = false;
  OTcurrentSystemState.OpenThermVersionSlave = 4.0f;
  r56 = isMsgIdReservedInActiveProfile(56);
  r57 = isMsgIdReservedInActiveProfile(57);
  printf("v4.x: MsgID 56 TdhwSet %s, MsgID 57 MaxTSet %s\n",
         r56 ? "SUPPRESSED" : "decoded", r57 ? "SUPPRESSED" : "decoded");
  printf("%s (%d mismatches)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
