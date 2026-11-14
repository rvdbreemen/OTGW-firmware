// TASK-1774 host harness driver: the RTC snapshot that carries settings.ini, reboot_count.txt and
// reboot_log.txt across a reset during a filesystem OTA (ADR-233). Built by test_fs_ota_rtc_snapshot.py,
// which extracts the code under test from the revision into generated *.inc files (never copied):
//  - the fsOtaRtcSnapshot block (OTGW-ModUpdateServer-esp32.h), absent before the fix (then stubs);
//  - the fsCommitTempFile block (helperStuff.ino);
//  - writeSettings() (settingStuff.ino) with its key lines replaced by a stub body;
//  - updateRebootCount() (helperStuff.ino).
// The driver plays the upload server's order of calls (fill before the unmount, arm at the first chunk,
// the image replacing the partition, the restore at the end, the boot restore before readSettings()); the
// Python side checks on the source text that the upload server and setup() call them in that order.
//
// LittleFS model as in test_settings_atomic_write.cpp (esp_littlefs 1.20.3: open "w" truncates at once,
// content commits at close, rename replaces in one commit). RTC_NOINIT memory survives the modelled reset
// because the driver keeps the process alive; only the filesystem and the RAM settings are reset.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define PSTR(s) (s)
#define DebugTf(...) ((void)0)
#define DebugTln(...) ((void)0)
#define DebugT(...) ((void)0)
#define Debugln(...) ((void)0)
#define RTC_NOINIT_ATTR

struct FsModel {
  std::map<std::string, std::string> files;
  std::map<std::string, int> openCount;
};
static FsModel fs;

struct Handle {
  std::string path;
  bool writing = false;
  bool open = false;
  std::string buf;
  size_t pos = 0;
};

class File {
 public:
  File() {}
  explicit File(std::shared_ptr<Handle> h) : h_(std::move(h)) {}
  explicit operator bool() const { return h_ && h_->open; }
  size_t write(const uint8_t *p, size_t n) {
    if (!h_ || !h_->open || !h_->writing) return 0;
    h_->buf.append(reinterpret_cast<const char *>(p), n);
    return n;
  }
  size_t print(const char *s) { return write(reinterpret_cast<const uint8_t *>(s), strlen(s)); }
  size_t print(const __FlashStringHelper *s) { return print(reinterpret_cast<const char *>(s)); }
  size_t printf_P(const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    return print(tmp);
  }
  size_t println(uint32_t v) {
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%u\r\n", static_cast<unsigned>(v));
    return print(tmp);
  }
  size_t position() const { return h_ ? (h_->writing ? h_->buf.size() : h_->pos) : 0; }
  size_t size() const { return h_ ? h_->buf.size() : 0; }
  int available() const { return (h_ && h_->open && !h_->writing) ? static_cast<int>(h_->buf.size() - h_->pos) : 0; }
  int read() {
    if (!available()) return -1;
    return static_cast<unsigned char>(h_->buf[h_->pos++]);
  }
  size_t read(uint8_t *p, size_t n) {
    size_t i = 0;
    for (; i < n && available(); i++) p[i] = static_cast<uint8_t>(read());
    return i;
  }
  size_t readBytesUntil(char term, char *p, size_t n) {
    size_t i = 0;
    while (i < n && available()) {
      int c = read();
      if (c == term) break;
      p[i++] = static_cast<char>(c);
    }
    return i;
  }
  void close() {
    if (!h_ || !h_->open) return;
    if (h_->writing) fs.files[h_->path] = h_->buf;
    fs.openCount[h_->path]--;
    h_->open = false;
  }
 private:
  std::shared_ptr<Handle> h_;
};

struct LittleFSModel {
  bool begin() { return true; }
  void end() {}
  bool exists(const char *p) { return fs.files.count(p) != 0; }
  File open(const char *p, const char *mode) {
    auto h = std::make_shared<Handle>();
    h->path = p;
    if (mode[0] == 'w') {
      fs.files[p] = "";
      h->writing = true;
    } else {
      if (!exists(p)) return File();
      h->buf = fs.files[p];
    }
    h->open = true;
    fs.openCount[p]++;
    return File(h);
  }
  bool remove(const char *p) { return fs.files.erase(p) != 0; }
  bool rename(const char *a, const char *b) {
    if (!exists(a) || fs.openCount[a] > 0 || fs.openCount[b] > 0) return false;
    fs.files[b] = fs.files[a];
    fs.files.erase(a);
    return true;
  }
};
static LittleFSModel LittleFS;
static bool LittleFSmounted = true;

// esp_rom_crc32_le() as in the ROM (the zlib CRC-32: reflected 0xEDB88320, ~crc in and out).
static uint32_t esp_rom_crc32_le(uint32_t crc, const uint8_t *buf, uint32_t len) {
  crc = ~crc;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

struct { void write(int) {} } debugTelnet;
struct { struct { bool bFsImageWriting = false; } flash; } state;
struct { char sHostname[32] = "otgw"; char sHTTPpasswd[32] = ""; } settings;
static bool settingsJsonFirstEntry = true;
static void safebootSyncNvs(const char *, const char *) {}

#include "defines.inc"
#include "helper.inc"
#include "snapshot.inc"

#ifndef FS_OTA_RTC_MAGIC   // the revision has no snapshot (before the fix): the calls do nothing
static void fsOtaRtcFill() {}
static void fsOtaRtcArm() {}
static void fsOtaRtcWipe() {}
static void fsOtaRtcDropSettings() {}
static void fsOtaRtcRestoreAtBoot(bool) {}
static bool fsOtaRtcArmed() { return false; }
#endif

// RAM settings: what writeSettings() writes. The body stands in for the ~200 key lines.
static std::string g_ramSettings = std::string(5600, 'u');
static void writeSettingsBody(File &file) { file.print(g_ramSettings.c_str()); }

#include "settings.inc"
#include "rebootcount.inc"

static const char *SET = "/settings.ini";
static const char *CNT = "/reboot_count.txt";
static const char *LOG = "/reboot_log.txt";
static const std::string IMAGE_DEFAULT = "{\n  \"hostname\": \"OTGW2\",\n  \"MQTTbroker\": \"homeassistant.local\"\n}\n";

// The unit before the filesystem OTA: the user's settings as the last save wrote them, a counter and a log.
static std::string g_userFile;
static void unitBefore(size_t settingsBytes = 5600) {
  fs = FsModel();
  LittleFSmounted = true;
  g_ramSettings = std::string(settingsBytes, 'u');
  writeSettings(false);
  g_userFile = fs.files[SET];
  fs.files[CNT] = "41\r\n";
  fs.files[LOG] = std::string(1450, 'L');
}

// The upload server's steps (OTGW-ModUpdateServer-esp32.h), in the order the Python side checks.
static void uploadStart() { fsOtaRtcFill(); LittleFSmounted = false; }
static void firstChunk() { fsOtaRtcArm(); }
static void imageLanded() { fs = FsModel(); fs.files[SET] = IMAGE_DEFAULT; fs.files["/v2.html.gz"] = "ui"; }
static void restoreAtEnd() {   // _restoreRuntimeFiles(): settings from RAM, the kept files, then the wipe
  LittleFSmounted = true;
  bool saved = writeSettings(false);
  fs.files[CNT] = "41\r\n";
  fs.files[LOG] = std::string(1450, 'L');
#ifdef FS_OTA_RTC_MAGIC
  if (saved) fsOtaRtcWipe();
#else
  (void)saved;
#endif
}

// A reset: RAM is lost (settings come back from the file), RTC_NOINIT and the filesystem stay.
struct BootResult { std::string settingsFile; uint32_t count; std::string log; };
static BootResult rebootUnit() {
  LittleFSmounted = true;
  g_ramSettings.clear();
  fsOtaRtcRestoreAtBoot(LittleFSmounted);
  BootResult r;
  r.settingsFile = fs.files.count(SET) ? fs.files[SET] : std::string("(missing)");   // what readSettings() reads
  r.count = updateRebootCount();
  r.log = fs.files.count(LOG) ? fs.files[LOG] : std::string("(missing)");
  return r;
}

static std::string what(const std::string &s) {
  if (s == g_userFile) return "the user's settings";
  if (s == IMAGE_DEFAULT) return "the image's default settings.ini";
  return "other (" + std::to_string(s.size()) + " B)";
}

static int report(const char *id, bool ok, const char *title, const std::string &got) {
  printf("CASE %s %s\n     %s\n     got : %s\n", id, ok ? "pass" : "FAIL", title, got.c_str());
  return ok ? 0 : 1;
}

static bool rtcClean() {
#ifdef FS_OTA_RTC_MAGIC
  if (g_fsOtaRtc.magic != 0) return false;
  for (size_t i = 0; i < sizeof(g_fsOtaRtc.data); i++)
    if (g_fsOtaRtc.data[i] != 0) return false;
  for (size_t i = 0; i < FS_OTA_RTC_SLOTS; i++)
    if (g_fsOtaRtc.len[i] != 0) return false;
#endif
  return true;
}

int main(int argc, char **argv) {
  const std::string c = argc > 1 ? argv[1] : "";
#ifdef FS_OTA_RTC_MAGIC
  memset(&g_fsOtaRtc, 0xA5, sizeof(g_fsOtaRtc));   // RTC garbage after power-on
#endif
  if (c == "T1") {   // a reset during the write, after the image replaced the partition
    unitBefore();
    uploadStart(); firstChunk(); imageLanded();
    BootResult r = rebootUnit();
    bool ok = r.settingsFile == g_userFile && r.count == 42 && r.log.size() >= 1450;
    return report("T1", ok, "a reset during the write keeps the user's settings, the counter (41 -> 42) and the log",
                  what(r.settingsFile) + ", count " + std::to_string(r.count) + ", log " + std::to_string(r.log.size()) + " B");
  }
  if (c == "T2") {   // a complete filesystem OTA, then a normal reboot
    unitBefore();
    uploadStart(); firstChunk(); imageLanded();
    g_ramSettings = std::string(5600, 'u');
    restoreAtEnd();
    bool armed = fsOtaRtcArmed();
    BootResult r = rebootUnit();
    bool ok = !armed && rtcClean() && r.settingsFile == g_userFile && r.count == 42;
    return report("T2", ok, "a complete filesystem OTA restores from RAM and leaves no armed snapshot",
                  std::string(armed ? "armed after restore, " : "disarmed, ") + what(r.settingsFile) +
                  ", count " + std::to_string(r.count));
  }
  if (c == "T3") {   // the restore wrote settings.ini, then a reset before the wipe at its end
    unitBefore();
    uploadStart(); firstChunk(); imageLanded();
    LittleFSmounted = true;
    g_ramSettings = std::string(5600, 'n');   // RAM is newer than the snapshot (the user changed a setting)
    writeSettings(false);
    std::string written = fs.files[SET];
    BootResult r = rebootUnit();
    bool ok = r.settingsFile == written;
    return report("T3", ok, "a reset after the restore's settings write keeps that write, not the older snapshot",
                  r.settingsFile == written ? "the restore's write" : what(r.settingsFile));
  }
  if (c == "T4") {   // a failed OTA (no restore), the user saves settings later, then a reset
    unitBefore();
    uploadStart(); firstChunk(); imageLanded();
    LittleFSmounted = true;
    g_ramSettings = std::string(5600, 'n');
    writeSettings(false);
    std::string later = fs.files[SET];
    BootResult r = rebootUnit();
    bool ok = r.settingsFile == later && r.count == 42;
    return report("T4", ok, "a settings save after a failed OTA wins over the snapshot; the counter is still kept",
                  (r.settingsFile == later ? std::string("the later save") : what(r.settingsFile)) +
                  ", count " + std::to_string(r.count));
  }
  if (c == "T5") {   // an upload refused before any byte (too large, Update.begin() failed): never armed
    unitBefore();
    uploadStart();
    fsOtaRtcWipe();
    LittleFSmounted = true;
    bool clean = rtcClean();
    BootResult r = rebootUnit();
    bool ok = clean && r.settingsFile == g_userFile && r.count == 42;
    return report("T5", ok, "an upload refused before the first chunk leaves nothing in RTC and nothing restored",
                  std::string(clean ? "RTC clean, " : "RTC holds data, ") + what(r.settingsFile));
  }
  if (c == "T6") {   // settings.ini larger than the snapshot: not protected, the counter and log still are
    unitBefore(7500);
    uploadStart(); firstChunk(); imageLanded();
    BootResult r = rebootUnit();
    bool ok = r.settingsFile == IMAGE_DEFAULT && r.count == 42 && r.log.size() >= 1450;
    return report("T6", ok, "a settings.ini that does not fit is not kept partly; the counter and log are kept",
                  what(r.settingsFile) + ", count " + std::to_string(r.count));
  }
  if (c == "T7") {   // a damaged snapshot is ignored
    unitBefore();
    uploadStart(); firstChunk();
#ifdef FS_OTA_RTC_MAGIC
    g_fsOtaRtc.data[10] ^= 0x40;
#endif
    imageLanded();
    BootResult r = rebootUnit();
    bool ok = r.settingsFile == IMAGE_DEFAULT && rtcClean();
    return report("T7", ok, "a snapshot with a bad CRC restores nothing and is wiped",
                  what(r.settingsFile) + (rtcClean() ? ", RTC clean" : ", RTC holds data"));
  }
  if (c == "T8") {   // the counter and log on flash are not replaced; the snapshot is wiped after the boot
    unitBefore();
    uploadStart(); firstChunk(); imageLanded();
    fs.files[CNT] = "77\r\n";
    fs.files[LOG] = std::string(30, 'N');
    BootResult r = rebootUnit();
    bool ok = r.count == 78 && r.log.size() == 30 && rtcClean();
    return report("T8", ok, "an existing counter and log are kept; the snapshot (with secrets) is wiped at boot",
                  "count " + std::to_string(r.count) + ", log " + std::to_string(r.log.size()) + " B" +
                  (rtcClean() ? ", RTC clean" : ", RTC holds data"));
  }
  printf("unknown case %s\n", c.c_str());
  return 2;
}
