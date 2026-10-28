//=======================================================================
// test_picSettingValue.cpp
//
// Host tests for copyPicSettingValue() (TASK-1430, GH #688), compiled
// against the REAL src/OTGW-firmware/picSettingValue.h.
//
// The defect: handlePRresponse() stored a PR= answer byte for byte. On a
// field gateway (1.7.8-beta.5, PIC 6.8) PR=Q answered with the single byte
// 0xFF, so otgw-pic/settings/reset_cause carried b'\xff'. Home Assistant
// could not decode it as UTF-8 and logged a warning on every publish, and
// /api/v2/pic/settings put the same byte into its JSON.
//
// What is real here: copyPicSettingValue(). The buffer sizes are the ones in
// OTGW-firmware.h (sResetCause is 8 bytes since TASK-1430; it was 4, too
// small for "0xFF"). The call site in handlePRresponse() is not compiled
// here.
//=======================================================================

#include "../../src/OTGW-firmware/picSettingValue.h"

#include <cstdio>
#include <cstring>

static int g_checks = 0;
static int g_failures = 0;

static void checkEq(const char* got, const char* want, const char* what) {
  g_checks++;
  if (std::strcmp(got, want) == 0) {
    std::printf("  [ OK ] %s\n", what);
  } else {
    g_failures++;
    std::printf("  [FAIL] %s: got \"", what);
    for (const unsigned char* p = (const unsigned char*)got; *p; p++) {
      if (*p >= 0x20 && *p <= 0x7E) std::printf("%c", *p); else std::printf("\\x%02X", *p);
    }
    std::printf("\", want \"%s\"\n", want);
  }
}

static bool isPrintableAscii(const char* s) {
  for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
    if (*p < 0x20 || *p > 0x7E) return false;
  }
  return true;
}

int main() {
  std::printf("test_picSettingValue\n");
  char buf[8];

  // GH #688: the reported answer.
  copyPicSettingValue(buf, sizeof(buf), "\xFF");
  checkEq(buf, "0xFF", "PR=Q answer 0xFF is stored as the text 0xFF");
  g_checks++;
  if (isPrintableAscii(buf)) std::printf("  [ OK ] stored value is printable ASCII (valid UTF-8)\n");
  else { g_failures++; std::printf("  [FAIL] stored value is not printable ASCII\n"); }

  // A normal reset cause is a single letter and must not change.
  copyPicSettingValue(buf, sizeof(buf), "P");
  checkEq(buf, "P", "printable reset cause P is stored unchanged");
  copyPicSettingValue(buf, sizeof(buf), "W");
  checkEq(buf, "W", "printable reset cause W is stored unchanged");

  // Other registers go through the same call.
  char clk[8];
  copyPicSettingValue(clk, sizeof(clk), "4");
  checkEq(clk, "4", "printable clock value is stored unchanged");
  char date[24];
  copyPicSettingValue(date, sizeof(date), "17:07 21-12-2023");
  checkEq(date, "17:07 21-12-2023", "printable build date with spaces is stored unchanged");

  // A mix of printable and other bytes is shown as hex, every byte.
  copyPicSettingValue(buf, sizeof(buf), "1\x01");
  checkEq(buf, "0x3101", "mixed answer 1 + 0x01 is shown as hex of every byte");

  // Hex never splits a byte when it does not fit: 8 bytes hold 0x + 2 bytes.
  copyPicSettingValue(buf, sizeof(buf), "\xFE\xFD\xFC");
  checkEq(buf, "0xFEFD", "hex is cut at a whole byte when the buffer is full");

  // A printable answer longer than the buffer is cut, as before.
  copyPicSettingValue(buf, sizeof(buf), "ABCDEFGHIJ");
  checkEq(buf, "ABCDEFG", "long printable answer is truncated to the buffer");

  // Empty stays empty (publishAllPICsettings() skips empty fields).
  copyPicSettingValue(buf, sizeof(buf), "");
  checkEq(buf, "", "empty answer stays empty");

  // Too small for any hex: store nothing rather than half a byte.
  char tiny[4];
  copyPicSettingValue(tiny, sizeof(tiny), "\xFF");
  checkEq(tiny, "", "buffer too small for 0xHH stores an empty value");

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
