// test/host/test_pic_banner_dispatch.cpp  (TASK-1179 AC#2)
//
// A PR=A reply's bytes go through the REAL OTGWSerial::matchBanner(); a
// completed banner calls the REAL fwreportinfo() through the REAL
// registerFirmwareCallback() hook, and the loop's REAL reportPendingPICRxErrors()
// runs the REAL applyPICBannerInfo(). test_pic_banner_dispatch.py slices all of
// them (see its docstring). This file supplies only the platform macros, a
// declaration of the OTGWSerial class with the members those slices use, and
// I/O doubles.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <string>

#define HAS_PIC 1
#define PROGMEM
#define PGM_P const char*
#define F(s) (s)
#define PSTR(s) (s)
#define pgm_read_byte(p) (*(const unsigned char*)(p))
#define snprintf_P snprintf
#define DebugT(...) ((void)0)
#define DebugTln(...) ((void)0)
#define DebugTf(...) ((void)0)
#define OTDebugTf(...) ((void)0)
typedef unsigned char byte;

static size_t strlcpy(char* dst, const char* src, size_t size) {
  size_t n = std::strlen(src);
  if (size) { size_t c = n < size - 1 ? n : size - 1; std::memcpy(dst, src, c); dst[c] = '\0'; }
  return n;
}

// ---- OTGWSerial library: enum and callback type, sliced ------------------------
#include "dispatch_lib_types.inc"

// The library class, declared with the members the sliced methods use. The two
// string reporters feed only the log and MQTT text, so they are doubles.
class OTGWSerial {
public:
  const char* firmwareVersion();
  void registerFirmwareCallback(OTGWFirmwareReport* func);
  void matchBanner(char ch);
  std::string processorToString() { return "pic16f1847"; }
  std::string firmwareToString() { return "gateway"; }
private:
  OTGWFirmwareReport* _firmwareFunc = nullptr;
  byte _banner_matched[FIRMWARE_COUNT] = {}, _version_pos = 0;
};

// ---- OTGWSerial library: file statics, banner table and methods, sliced --------
#include "dispatch_lib_impl.inc"

OTGWSerial OTGWSerial;   // the firmware's object carries the class's name (OTGW-firmware.h:78)

// ---- firmware surface the slices touch --------------------------------------------
enum HardwareMode { HW_MODE_PIC, HW_MODE_OT_DIRECT, HW_MODE_DEGRADED };
struct StateStub {
  struct { bool bAvailable; char sFwversion[16]; char sDeviceid[16]; char sType[16]; } pic;
  struct { HardwareMode eMode; } hw;
} state;
struct { uint16_t errorBufferOverflow; } OTcurrentSystemState;   // uint16_t as in OTGW-Core.h
static char cMsg[128];
static void reportOTGWEvent_P(PGM_P, char, bool = false) {}
static void sendEventToWebSocket(char, const char*) {}
static void sendMQTTData(const char*, const char*) {}
static char* utoa(unsigned v, char* buf, int) { std::snprintf(buf, 12, "%u", v); return buf; }

static int  g_publishes = 0;
static bool g_enabledAtPublish = false;
void sendMQTTversioninfo();                               // recording double, defined below
void fwreportinfo(OTGWFirmware fw, const char* version);  // the firmware gets this from the .ino preprocessor

// ---- code under test from the firmware, sliced ------------------------------------
#include "dispatch_fw.inc"

void sendMQTTversioninfo() { g_publishes++; g_enabledAtPublish = isPICEnabled(); }

// ---- harness ------------------------------------------------------------------------
// What OTGWSerial::read() does with every byte it returns (OTGWSerial.cpp: read()).
static void picSends(const char* s) { for (const char* p = s; *p; ++p) OTGWSerial.matchBanner(*p); }

static void boot(bool picFound) {
  state.pic.bAvailable = picFound;
  state.hw.eMode = picFound ? HW_MODE_PIC : HW_MODE_DEGRADED;
  state.pic.sFwversion[0] = state.pic.sDeviceid[0] = state.pic.sType[0] = '\0';
  g_publishes = 0; g_enabledAtPublish = false;
  OTGWSerial.registerFirmwareCallback(fwreportinfo);     // as OTGW-Core.ino does at setup
}

static int g_failures = 0;
static void verdict(const char* id, const char* title, bool wantAvailable, const char* wantVersion,
                    int wantPublishes, bool wantEnabledAtPublish) {
  const bool ok = state.pic.bAvailable == wantAvailable &&
                  (state.hw.eMode == HW_MODE_PIC) == wantAvailable &&
                  std::strcmp(state.pic.sFwversion, wantVersion) == 0 &&
                  g_publishes == wantPublishes &&
                  (wantPublishes == 0 || g_enabledAtPublish == wantEnabledAtPublish);
  std::printf("CASE %s %s\n     %s\n", id, ok ? "pass" : "FAIL", title);
  std::printf("     got : available=%d mode=%s fwversion='%s' publishes=%d enabled_at_publish=%d\n",
              (int)state.pic.bAvailable, state.hw.eMode == HW_MODE_PIC ? "PIC" : "DEGRADED",
              state.pic.sFwversion, g_publishes, (int)g_enabledAtPublish);
  if (!ok)
    std::printf("     want: available=%d fwversion='%s' publishes=%d enabled_at_publish=%d\n",
                (int)wantAvailable, wantVersion, wantPublishes, (int)wantEnabledAtPublish);
  if (!ok) g_failures++;
}

int main() {
  std::printf("== PR=A reply through the real banner dispatch (TASK-1179 AC#2) ==\n");
  {
    boot(false);
    picSends("PR: A=OpenTherm Gateway 6.6\r\n");
    reportPendingPICRxErrors();
    verdict("D1", "boot probe missed the PIC; PR=A reply 'PR: A=OpenTherm Gateway 6.6': PIC re-enabled, otgw-pic/* published",
            true, "6.6", 1, true);
  }
  {
    boot(false);
    picSends("PR: G=12\r\n");
    reportPendingPICRxErrors();
    verdict("D2", "control: a PR reply without a banner changes nothing and publishes nothing",
            false, "", 0, false);
  }
  {
    boot(false);
    picSends("Opentherm gateway diagnostics - Version 2.2\r\n");
    reportPendingPICRxErrors();
    verdict("D3", "boot probe missed the PIC; the diagnostics firmware banner: PIC re-enabled, published",
            true, "2.2", 1, true);
  }
  {
    boot(true);
    picSends("PR: A=OpenTherm Gateway 6.6\r\n");
    reportPendingPICRxErrors();
    verdict("D4", "control: PIC found at boot; PR=A reply: stays enabled, published",
            true, "6.6", 1, true);
  }
  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
