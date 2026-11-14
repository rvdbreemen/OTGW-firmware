// TASK-1904 host harness driver: what updateRebootLog() writes for a given reset. Built by
// test_reboot_log_reset_raw.py, which extracts updateRebootLog() and copyOlderRebootLogLines() (helperStuff.ino)
// from the revision under test into generated *.inc files (never copied). The hardware calls are stubs the
// cases set: the mapped reset reason, the raw per-core codes (lastResetRaw, filled by setup() in the firmware)
// and the 0x26 reset flag (g_extWdResetAtBoot, read in setup() in the firmware). On the ESP32-S3 the mapping has
// no ESP_RST_EXT, so the stub platformIsExternalReset() is false as on the device.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <string>

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define PSTR(s) (s)
#define DebugTf(...) ((void)0)
#define Debugf(...) ((void)0)
#define DebugTln(...) ((void)0)
#define snprintf_P snprintf
#define strlcpy_P strlcpy
#define CSTR(x) ((x).c_str())
#define REBOOT_LOG_LINE_MAX 240

static size_t strlcpy(char *d, const char *s, size_t n) {
  size_t l = strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; }
  return l;
}

struct String {
  std::string s;
  String(const char *c) : s(c) {}
  const char *c_str() const { return s.c_str(); }
};

static std::map<std::string, std::string> g_files;

struct Handle { std::string path; bool writing = false; bool open = false; std::string buf; size_t pos = 0; };

class File {
 public:
  File() {}
  explicit File(std::shared_ptr<Handle> h) : h_(std::move(h)) {}
  explicit operator bool() const { return h_ && h_->open; }
  size_t print(const char *s) { if (h_ && h_->writing) h_->buf += s; return strlen(s); }
  size_t print(const __FlashStringHelper *s) { return print(reinterpret_cast<const char *>(s)); }
  int available() const { return (h_ && h_->open && !h_->writing) ? (int)(h_->buf.size() - h_->pos) : 0; }
  int read() { return available() ? (unsigned char)h_->buf[h_->pos++] : -1; }
  int peek() const { return available() ? (unsigned char)h_->buf[h_->pos] : -1; }
  void close() { if (h_ && h_->open) { if (h_->writing) g_files[h_->path] = h_->buf; h_->open = false; } }
 private:
  std::shared_ptr<Handle> h_;
};

struct LittleFSModel {
  bool begin() { return true; }
  bool exists(const char *p) { return g_files.count(p) != 0; }
  bool remove(const char *p) { return g_files.erase(p) != 0; }
  File open(const char *p, const char *mode) {
    auto h = std::make_shared<Handle>();
    h->path = p;
    if (mode[0] == 'w') { g_files[p] = ""; h->writing = true; }
    else { if (!exists(p)) return File(); h->buf = g_files[p]; }
    h->open = true;
    return File(h);
  }
  bool rename(const char *a, const char *b) { if (!exists(a)) return false; g_files[b] = g_files[a]; g_files.erase(a); return true; }
};
static LittleFSModel LittleFS;

struct { struct { bool bFsImageWriting = false; } flash; } state;
struct { struct { std::string sTimezone = "Europe/Amsterdam"; } ntp; } settings;
struct TimeZone {};
struct { TimeZone createForZoneName(const char *) { return TimeZone(); } } timezoneManager;
struct ZonedDateTime {
  static ZonedDateTime forUnixSeconds64(int64_t, TimeZone) { return ZonedDateTime(); }
  int year() const { return 2026; } int month() const { return 11; } int day() const { return 14; }
  int hour() const { return 10; } int minute() const { return 0; } int second() const { return 0; }
};

// --- the device as the case sets it
static uint32_t g_resetCode = 1;            // esp_reset_reason(): 1 = ESP_RST_POWERON
static std::string g_exceptionText;          // "- Access to invalid address (29)" style, or empty
static bool g_clockValid = true;
static char lastResetRaw[16] = "";
static bool g_extWdResetAtBoot = false;
static bool g_bootRestartReasonValid = false;
static char g_bootRestartReason[48] = {0};

static void loopNTP() {}
static uint32_t platformResetCode() { return g_resetCode; }
static void platformResetRegisterDump(char *buf, size_t) { buf[0] = 0; }
static void platformResetExceptionInfo(char *buf, size_t len) { strlcpy(buf, g_exceptionText.c_str(), len); }
static bool platformIsExternalReset() { return false; }   // ESP32-S3: no ESP_RST_EXT in its reset mapping
static bool platformResetWasSoftware() { return false; }
static void initWatchDog(char *buf, size_t len) { strlcpy(buf, "Reset by External WD\r\n", len); }
static bool rebootLogClockValid(int64_t) { return g_clockValid; }

#include "rebootlog.inc"

static std::string logText() { return g_files.count("/reboot_log.txt") ? g_files["/reboot_log.txt"] : std::string(); }

static int report(const char *id, bool ok, const char *what, const std::string &got) {
  printf("CASE %s %s\n     %s\n     got : %s\n", id, ok ? "pass" : "FAIL", what, got.c_str());
  return ok ? 0 : 1;
}

static std::string firstLines(const std::string &t, int n) {
  std::string out;
  size_t p = 0;
  for (int i = 0; i < n && p < t.size(); i++) {
    size_t e = t.find("\r\n", p);
    if (e == std::string::npos) e = t.size();
    out += (i ? " | " : "") + t.substr(p, e - p);
    p = e + 2;
  }
  return out;
}

int main(int argc, char **argv) {
  const std::string c = argc > 1 ? argv[1] : "";
  g_files["/reboot_log.txt"] = "2026-11-13 09:00:00 - reboot cause: Software reset (3)\r\n";
  if (c == "E1") {   // the 0x26 reset the ESP: the chip pulls the reset line, the S3 reports power-on (0x01)
    g_resetCode = 1; strlcpy(lastResetRaw, "0x01/0x01", sizeof(lastResetRaw)); g_extWdResetAtBoot = true;
    updateRebootLog(String("Power on"));
    std::string t = logText();
    return report("E1", t.find("External watchdog 0x26") != std::string::npos,
                  "a reset by the external 0x26 watchdog is in the reboot log", firstLines(t, 3));
  }
  if (c == "E2") {   // 'Other watchdog' (ESP_RST_WDT = 4) from an RTC watchdog: the raw code tells which one
    g_resetCode = 4; strlcpy(lastResetRaw, "0x10/0x10", sizeof(lastResetRaw));
    updateRebootLog(String("Other watchdog"));
    std::string t = logText();
    return report("E2", t.find("[rst 0x10/0x10]") != std::string::npos,
                  "the boot line carries the raw per-core reset code next to the mapped text", firstLines(t, 1));
  }
  if (c == "E3") {   // a plain power-on without the 0x26 flag: no watchdog line
    g_resetCode = 1; strlcpy(lastResetRaw, "0x01/0x01", sizeof(lastResetRaw)); g_extWdResetAtBoot = false;
    updateRebootLog(String("Power on"));
    std::string t = logText();
    return report("E3", t.find("External watchdog") == std::string::npos && t.find("Reset by External WD") == std::string::npos,
                  "without the 0x26 flag no external-watchdog line", firstLines(t, 2));
  }
  if (c == "E4") {   // the longest boot line still ends whole: longest text, longest exception, clock not set or set
    g_resetCode = 2; strlcpy(lastResetRaw, "0x11/0x11", sizeof(lastResetRaw));
    g_exceptionText = std::string("- ") + std::string(61, 'x');   // the 64 B buffer's worth
    updateRebootLog(String("Interrupt watchdog"));
    std::string t = logText();
    std::string first = t.substr(0, t.find("\r\n"));
    // The boot line must end at its own CRLF (no run-in with the next line) and keep the raw code.
    bool ok = first.find("[rst 0x11/0x11]") != std::string::npos && first.size() + 2 <= 160 &&
              first.find("reboot cause: Software reset") == std::string::npos;
    return report("E4", ok, "the longest boot line keeps the raw code, ends with its own CRLF and fits 160 B",
                  std::to_string(first.size()) + " chars: " + first.substr(0, 90));
  }
  printf("unknown case %s\n", c.c_str());
  return 2;
}
