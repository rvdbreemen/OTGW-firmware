// test/host/test_pic_banner_recovery.cpp  (TASK-1179)
//
// A PIC that detectPIC() missed at boot leaves state.pic.bAvailable false and
// hw.eMode DEGRADED. The 60 s PR=A probe's reply only reaches the firmware
// banner callback, whose loop-side consumer is applyPICBannerInfo() (TASK-1175).
// This harness runs that REAL consumer (sliced from OTGW-Core.ino by
// test_pic_banner_recovery.py, together with the real isPICEnabled() from
// OTGW-firmware.h) against a DEGRADED state and checks that it re-enables the
// PIC, and that the version publish already sees the PIC as enabled.
//
// Test doubles only: the OTGWSerial banner getters, the state struct fields the
// function touches, the debug macros and a recording sendMQTTversioninfo().

#include <cstdio>
#include <cstring>
#include <string>

#define HAS_PIC 1
#define F(s) (s)
#define PSTR(s) (s)
#define DebugTln(...) ((void)0)
#define DebugTf(...) ((void)0)
#define OTDebugTf(...) ((void)0)

static size_t strlcpy(char* dst, const char* src, size_t size) {
  size_t n = std::strlen(src);
  if (size) { size_t c = n < size - 1 ? n : size - 1; std::memcpy(dst, src, c); dst[c] = '\0'; }
  return n;
}

enum HardwareMode { HW_MODE_PIC, HW_MODE_OT_DIRECT, HW_MODE_DEGRADED };
struct StateStub {
  struct { bool bAvailable; char sFwversion[16]; char sDeviceid[16]; char sType[16]; } pic;
  struct { HardwareMode eMode; } hw;
} state;

struct OTGWSerialStub {
  const char* firmwareVersion() { return "6.6"; }
  std::string processorToString() { return "pic16f1847"; }
  std::string firmwareToString() { return "gateway"; }
} OTGWSerial;

void sendMQTTversioninfo();                    // recording double, defined below
#include "generated/pic_banner_recovery.inc"   // isPICEnabled() + applyPICBannerInfo()

static int g_publishes = 0;
static bool g_enabledAtPublish = false;
void sendMQTTversioninfo() { g_publishes++; g_enabledAtPublish = isPICEnabled(); }

int main() {
  int failures = 0;
  auto check = [&](bool ok, const char* what) {
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
  };

  // Boot probe missed the PIC: DEGRADED, PIC functions off, device id unknown.
  state.pic.bAvailable = false;
  state.hw.eMode = HW_MODE_DEGRADED;
  strlcpy(state.pic.sDeviceid, "unknown", sizeof(state.pic.sDeviceid));

  applyPICBannerInfo();   // the PR=A reply's banner, applied loop-side

  check(state.pic.bAvailable, "banner re-enables the PIC (state.pic.bAvailable)");
  check(state.hw.eMode == HW_MODE_PIC, "hardware mode becomes HW_MODE_PIC");
  check(std::strcmp(state.pic.sDeviceid, "pic16f1847") == 0, "device id is filled (the 60 s probe stops)");
  check(g_publishes == 1, "version info is published once");
  check(g_enabledAtPublish, "the publish already sees isPICEnabled() == true (otgw-pic/* not skipped)");

  // A PIC that was already enabled stays enabled and in its mode.
  state.hw.eMode = HW_MODE_PIC;
  applyPICBannerInfo();
  check(state.pic.bAvailable && state.hw.eMode == HW_MODE_PIC, "an enabled PIC is left as it is");

  std::printf("%s (%d failure(s))\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
