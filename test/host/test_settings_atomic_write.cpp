// TASK-1913 host harness driver: writeSettings() and updateRebootCount() against a LittleFS model
// that can lose power in the middle of a write. Built by test_settings_atomic_write.py, which
// extracts the code under test from the revision into generated *.inc files (never copied).
//
// The model follows the sources cited in the task notes:
//  - open(path, "w") truncates and that truncation is on flash at once: esp_littlefs 1.20.3
//    vfs_littlefs_open() calls lfs_file_sync() for a file opened for writing;
//  - the written content reaches flash only at close() (CONFIG_LITTLEFS_FLUSH_FILE_EVERY_WRITE
//    is not set in the framework sdkconfig, and the Arduino VFS write() only fwrite()s);
//  - rename(a, b) replaces an existing b in one commit (littlefs lfs_rename_), and fails with
//    EBUSY while a or b is open (esp_littlefs vfs_littlefs_rename()).
// A power cut is a C++ exception thrown from inside a write; open handles commit nothing.
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

struct PowerCut {};

struct FsModel {
  std::map<std::string, std::string> files;   // what is on flash
  std::map<std::string, int> openCount;
  size_t written = 0;                           // bytes accepted by writes since the case started
  size_t cutAt = SIZE_MAX;                      // power is lost when this many bytes were written
  size_t spaceLeft = SIZE_MAX;                  // a full filesystem accepts only this many more bytes
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
    size_t done = 0;
    for (; done < n; done++) {
      if (fs.written >= fs.cutAt) throw PowerCut{};
      if (fs.spaceLeft == 0) break;
      h_->buf.push_back(static_cast<char>(p[done]));
      fs.written++;
      if (fs.spaceLeft != SIZE_MAX) fs.spaceLeft--;
    }
    return done;
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
    if (h_->writing) fs.files[h_->path] = h_->buf;   // content commits at close
    fs.openCount[h_->path]--;
    h_->open = false;
  }
 private:
  std::shared_ptr<Handle> h_;
};

struct LittleFSModel {
  bool begin() { return true; }
  bool exists(const char *p) { return fs.files.count(p) != 0; }
  File open(const char *p, const char *mode) {
    auto h = std::make_shared<Handle>();
    h->path = p;
    if (mode[0] == 'w') {
      fs.files[p] = "";          // truncation is on flash at open (vfs_littlefs_open syncs)
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
    if (!exists(a) || fs.openCount[a] > 0 || fs.openCount[b] > 0) return false;   // EBUSY
    fs.files[b] = fs.files[a];   // one commit: the old b goes, a becomes b
    fs.files.erase(a);
    return true;
  }
};
static LittleFSModel LittleFS;

struct { void write(int) {} } debugTelnet;
struct { struct { bool bFsImageWriting = false; } flash; } state;
struct { char sHostname[32] = "otgw"; char sHTTPpasswd[32] = ""; } settings;
static bool settingsJsonFirstEntry = true;
static void safebootSyncNvs(const char *, const char *) {}

#include "defines.inc"

// The settings body stands in for the ~200 key lines of writeSettings(): plain bytes, so a
// power cut or a full filesystem can land anywhere inside it.
static size_t g_bodyBytes = 600;
static void writeSettingsBody(File &file) {
  std::string body(g_bodyBytes, 'k');
  file.print(body.c_str());
}

#include "helper.inc"
#include "settings.inc"
#include "rebootcount.inc"

static const char *SET = "/settings.ini";
static const char *OLD = "{\"old\":true}\n";

static void reset() {
  fs = FsModel();
  fs.files[SET] = OLD;
}

static bool noTempFiles() {
  for (auto &kv : fs.files)
    if (kv.first.find(".tmp") != std::string::npos) return false;
  return true;
}

static int report(const char *id, bool ok, const char *what, const std::string &got) {
  printf("CASE %s %s\n     %s\n     got : %s\n", id, ok ? "pass" : "FAIL", what, got.c_str());
  return ok ? 0 : 1;
}

static std::string describeSettings() {
  auto it = fs.files.find(SET);
  if (it == fs.files.end()) return "settings.ini missing";
  std::string s = it->second == OLD ? "old content" : ("new/other content, " + std::to_string(it->second.size()) + " B");
  return s + (noTempFiles() ? "" : ", temp file left");
}

int main(int argc, char **argv) {
  const std::string c = argc > 1 ? argv[1] : "";
  if (c == "W1") {   // power cut in the middle of the settings body
    reset();
    fs.cutAt = 100;
    try { writeSettings(false); } catch (PowerCut &) {}
    return report("W1", fs.files.count(SET) && fs.files[SET] == OLD,
                  "a power cut during writeSettings() leaves the old settings.ini", describeSettings());
  }
  if (c == "W2") {   // a normal save
    reset();
    bool r = writeSettings(false);
    bool ok = r && fs.files.count(SET) && fs.files[SET].size() > g_bodyBytes && fs.files[SET] != OLD && noTempFiles();
    return report("W2", ok, "a normal save replaces settings.ini and leaves no temp file",
                  std::string(r ? "true, " : "false, ") + describeSettings());
  }
  if (c == "W3") {   // the filesystem fills up during the save
    reset();
    fs.spaceLeft = 300;
    bool r = writeSettings(false);
    bool ok = !r && fs.files.count(SET) && fs.files[SET] == OLD && noTempFiles();
    return report("W3", ok, "a full filesystem fails the save and keeps the old settings.ini",
                  std::string(r ? "true, " : "false, ") + describeSettings());
  }
  if (c == "W4") {   // settings.ini is open elsewhere (a download) during the save
    reset();
    File reader = LittleFS.open(SET, "r");
    bool r = writeSettings(false);
    reader.close();
    bool ok = !r && fs.files.count(SET) && fs.files[SET] == OLD && noTempFiles();
    return report("W4", ok, "a save while settings.ini is open fails and keeps it (retried by the flush)",
                  std::string(r ? "true, " : "false, ") + describeSettings());
  }
  if (c == "R1") {   // power cut while the reboot counter is written
    reset();
    fs.files["/reboot_count.txt"] = "41\r\n";
    fs.cutAt = 1;
    try { updateRebootCount(); } catch (PowerCut &) {}
    auto it = fs.files.find("/reboot_count.txt");
    std::string got = it == fs.files.end() ? "missing" : ("\"" + it->second.substr(0, 8) + "\"");
    return report("R1", it != fs.files.end() && it->second == "41\r\n",
                  "a power cut while the counter is written keeps the old count", got);
  }
  if (c == "R2") {   // a normal boot
    reset();
    fs.files["/reboot_count.txt"] = "41\r\n";
    uint32_t n = updateRebootCount();
    auto it = fs.files.find("/reboot_count.txt");
    bool ok = n == 42 && it != fs.files.end() && it->second == "42\r\n" && noTempFiles();
    return report("R2", ok, "a normal boot counts 41 -> 42 and leaves no temp file",
                  std::to_string(n) + ", " + (it == fs.files.end() ? std::string("missing") : it->second.substr(0, 8)));
  }
  printf("unknown case %s\n", c.c_str());
  return 2;
}
