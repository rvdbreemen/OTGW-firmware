/*
***************************************************************************
**  Program  : test/host/test_unknown_counters_asan.cpp
**
**  AddressSanitizer proof for the 3-strike unknown-ID counters in
**  OTDirect.ino (TASK-1173). run_unknown_counters.ps1 -Asan slices the real
**  declaration and helpers into generated/*.inc and builds this file with
**  MSVC /fsanitize=address.
**
**  Unlike test_unknown_counters.cpp there is no guard region and no macro.
**  The sliced declaration is compiled as a real global, so ASan places a
**  redzone after it and reports any access past its end, naming
**  otUnknownCounters and the OTDirect.ino line of the declaration.
**
**  Every MsgID is printed and flushed as "id=<n>" before its calls, so the
**  last such line before an ASan report names the MsgID that went out of
**  bounds. Cases, in this order:
**   1. MsgIDs 0-127: four incUnknownCount() calls give getUnknownCount() == 3,
**      then clearUnknownCount() gives 0. handleMasterResponse() never passes
**      MsgID 0, but the helpers do not special-case it, so it is tested too.
**   2. MsgIDs 131, 132, 133, then 128-255: three incUnknownCount() calls,
**      then getUnknownCount() must be 0, then clearUnknownCount().
**  On the unbounded code the output stops at id=131 with an ASan
**  global-buffer-overflow report. On the fixed code every case passes and
**  the exit code is 0. run_unknown_counters.ps1 -Asan judges the transcript,
**  not only the exit code.
**
**  This build proves memory safety and that get() stays 0 for 128-255. The
**  guard-region build (the default) is the full regression gate: it also
**  catches a helper that stays inside the array but changes the counter of
**  another MsgID.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#include <cstdint>
#include <cstdio>

#include "generated/uc_labels.inc"      // UC_SUT_LABEL
#include "generated/uc_sut_decl.inc"    // the real otUnknownCounters declaration
#include "generated/uc_sut_funcs.inc"   // the real get/inc/clear helpers

static int failures = 0;

static void expectCount(unsigned id, const char* when, unsigned got, unsigned want) {
  if (got == want) return;
  failures++;
  printf("  FAIL MsgID %u: getUnknownCount() is %u %s, expected %u\n", id, got, when, want);
}

static void announce(unsigned id) {
  printf("id=%u\n", id);
  fflush(stdout);
}

static void outOfRangeCase(unsigned id) {
  announce(id);
  for (int n = 0; n < 3; n++) incUnknownCount((uint8_t)id);
  expectCount(id, "after 3 x inc", getUnknownCount((uint8_t)id), 0);
  clearUnknownCount((uint8_t)id);
}

int main() {
  printf("== otUnknownCounters bounds: AddressSanitizer run ==\n");
  printf("SUT: %s\n", UC_SUT_LABEL);
  printf("otUnknownCounters: %u bytes, a real global with an ASan redzone after it\n",
         (unsigned)sizeof(otUnknownCounters));

  printf("case 1: MsgIDs 0-127, 4 x inc gives 3, clear gives 0\n");
  fflush(stdout);
  for (unsigned id = 0; id <= 127; id++) {
    announce(id);
    for (int n = 0; n < 4; n++) incUnknownCount((uint8_t)id);
    expectCount(id, "after 4 x inc", getUnknownCount((uint8_t)id), 3);
    clearUnknownCount((uint8_t)id);
    expectCount(id, "after clear", getUnknownCount((uint8_t)id), 0);
  }

  printf("case 2: MsgIDs 131, 132, 133, then 128-255, 3 x inc must leave get() at 0\n");
  fflush(stdout);
  outOfRangeCase(131);
  outOfRangeCase(132);
  outOfRangeCase(133);
  for (unsigned id = 128; id <= 255; id++) outOfRangeCase(id);

  if (failures) printf("\n== %d case(s) failed ==\n", failures);
  else          printf("\n== all cases passed ==\n");
  fflush(stdout);
  return failures ? 1 : 0;
}
