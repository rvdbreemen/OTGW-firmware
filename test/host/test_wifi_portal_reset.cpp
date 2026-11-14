// TASK-1930 host harness: the reset-button WiFi portal trigger (ADR-043) on the ESP32-S3.
// Compiled by test_wifi_portal_reset.py with code extracted from the revision under test:
//   portal.inc   the WIFI_PORTAL_RESET_* block of OTGW-firmware.ino, from the #defines to wifiPortalResetWindowExpired()
//   extreset.inc platformIsExternalReset() of platform_esp32.h
// The S3 reset mapping is modelled from the pinned libs: the EN pin (reset button) and a power-on both give raw
// 0x01 (soc/esp32s3 reset_reasons.h:38-40), which esp_reset_reason() reports as ESP_RST_POWERON; it never reports
// ESP_RST_EXT. NVS (platformRtcRead/Write) survives every boot; RAM globals and millis() start at 0 on every boot.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define F(s) (s)
#define PSTR(s) (s)
#define SetupDebugTf(...) ((void)0)
#define SetupDebugTln(...) ((void)0)

typedef enum { ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_INT_WDT,
               ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT, ESP_RST_SDIO, ESP_RST_USB } esp_reset_reason_t;

static esp_reset_reason_t g_reason = ESP_RST_POWERON;
static unsigned g_raw = 0x01;
static uint32_t g_millis = 0;
static esp_reset_reason_t esp_reset_reason() { return g_reason; }
static uint32_t millis() { return g_millis; }
static unsigned platformResetRawCode(int) { return g_raw; }   // the TASK-1904 shim, for a fix that uses it

static std::map<uint32_t, std::vector<uint8_t>> g_nvs;   // survives every boot
static int g_nvsWrites = 0;
static bool platformRtcRead(uint32_t slot, uint32_t *data, size_t len) {
  auto it = g_nvs.find(slot);
  if (it == g_nvs.end() || it->second.size() != len) return false;
  memcpy(data, it->second.data(), len);
  return true;
}
static bool platformRtcWrite(uint32_t slot, const uint32_t *data, size_t len) {
  g_nvs[slot].assign(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + len);
  g_nvsWrites++;
  return true;
}

#include "extreset.inc"
#include "portal.inc"

enum Kind { BUTTON, POWER, SOFTWARE, EXT8266 };
static const char *kindName(Kind k) {
  return k == BUTTON ? "reset button" : k == POWER ? "power-on" : k == SOFTWARE ? "software reset" : "ESP_RST_EXT";
}

// One boot: the reset kind sets the reason the S3 reports, setup() asks the trigger, then loop() runs for uptimeMs
// and clears the count when the window expires (OTGW-firmware.ino loop()).
static bool boot(Kind k, uint32_t uptimeMs) {
  switch (k) {
    case BUTTON:   g_reason = ESP_RST_POWERON; g_raw = 0x01; break;   // S3: EN pin = chip power-on code
    case POWER:    g_reason = ESP_RST_POWERON; g_raw = 0x01; break;
    case SOFTWARE: g_reason = ESP_RST_SW;      g_raw = 0x0C; break;
    case EXT8266:  g_reason = ESP_RST_EXT;     g_raw = 0x01; break;   // what ADR-043 was written for (ESP8266)
  }
  g_millis = 0;
  wifiPortalResetWindowOpen = false;
  wifiPortalResetWindowDeadline = 0;
  const bool force = shouldForceWifiConfigPortal();
  for (uint32_t t = 0; t <= uptimeMs; t += 100) {
    g_millis = t;
    if (wifiPortalResetWindowExpired()) {
      clearWifiPortalResetState();
      wifiPortalResetWindowOpen = false;
      wifiPortalResetWindowDeadline = 0;
    }
  }
  return force;
}

static int report(const char *c, bool ok, const char *title, const std::string &got) {
  printf("CASE %s %s\n     %s\n     got : %s\n", c, ok ? "pass" : "FAIL", title, got.c_str());
  return ok ? 0 : 1;
}

static std::string run(const std::vector<std::pair<Kind, uint32_t>> &boots, std::vector<bool> &forced) {
  g_nvs.clear();
  std::string s;
  for (auto &b : boots) {
    const bool f = boot(b.first, b.second);
    forced.push_back(f);
    s += std::string(kindName(b.first)) + (f ? " -> PORTAL" : " -> normal") + "; ";
  }
  return s;
}

int main(int argc, char **argv) {
  const std::string c = argc > 1 ? argv[1] : "";
  std::vector<bool> f;
  if (c == "P1") {   // the ADR-043 gesture: press reset 3x, each press within the 10 s window of the boot before
    std::string got = run({{POWER, 60000}, {BUTTON, 3000}, {BUTTON, 3000}, {BUTTON, 3000}}, f);
    return report("P1", !f[0] && !f[1] && !f[2] && f[3],
                  "three reset-button presses within the window force the portal (expectation of options A/B)", got);
  }
  if (c == "P2") {   // software resets (OTA, /ReBoot, WiFi supervisor) must never count
    std::string got = run({{POWER, 60000}, {SOFTWARE, 3000}, {SOFTWARE, 3000}, {SOFTWARE, 3000}}, f);
    return report("P2", !f[1] && !f[2] && !f[3], "three software resets never force the portal", got);
  }
  if (c == "P3") {   // presses further apart than the window: the count is cleared in between
    std::string got = run({{POWER, 60000}, {BUTTON, 15000}, {BUTTON, 15000}, {BUTTON, 15000}}, f);
    return report("P3", !f[1] && !f[2] && !f[3], "three resets each after the 10 s window has expired never force the portal", got);
  }
  if (c == "P4") {   // the counting logic itself, fed with the reason it was written for
    std::string got = run({{POWER, 60000}, {EXT8266, 3000}, {EXT8266, 3000}, {EXT8266, 3000}}, f);
    return report("P4", !f[1] && !f[2] && f[3], "with ESP_RST_EXT (ESP8266 semantics) the counter does force the portal", got);
  }
  printf("unknown case %s\n", c.c_str());
  return 2;
}
