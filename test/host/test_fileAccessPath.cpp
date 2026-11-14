// TASK-1935 host harness: handleFile() (FSexplorer.ino) and the protected settings file.
// Compiled by test_fileAccessPath.py with handlefile.inc, the real handleFile() of the revision under test (and on
// the fix its fsCanonicalPath()), sliced from src/OTGW-firmware/FSexplorer.ino. The filesystem is the REAL littlefs
// of ESP8266 core 2.7.4 (lfs.c, lfs_util.c) on a RAM block device, so its path lookup is the one the device runs.
// One mechanical rewrite of the copy: the GCC statement expression on the last line of handleFile() becomes an
// immediately invoked lambda, because MSVC has no statement expressions (the .py asserts exactly one match).
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
extern "C" {
#include "lfs.h"
}

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define PSTR(s) (s)
#define PROGMEM
typedef const char *PGM_P;
#define strcasecmp_P _stricmp
#define DebugTf(...) ((void)0)
#define DebugTln(...) ((void)0)
#define SETTINGS_FILE "/settings.ini"

class String {
 public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  const char *c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  bool endsWith(const char *t) const {
    const size_t n = strlen(t);
    return s_.size() >= n && s_.compare(s_.size() - n, n, t) == 0;
  }
  String &operator=(const char *s) { s_ = s ? s : ""; return *this; }
  String &operator+=(const char *s) { s_ += s; return *this; }
  String &operator+=(const __FlashStringHelper *s) { s_ += reinterpret_cast<const char *>(s); return *this; }
  bool operator==(const char *s) const { return s_ == s; }
 private:
  std::string s_;
};

// ---- the real littlefs on a RAM block device -------------------------------------------------------------
static const int BS = 256, BC = 64;
static uint8_t g_flash[BS * BC];
static int bd_read(const lfs_config *, lfs_block_t b, lfs_off_t o, void *buf, lfs_size_t n) {
  memcpy(buf, g_flash + b * BS + o, n); return 0;
}
static int bd_prog(const lfs_config *, lfs_block_t b, lfs_off_t o, const void *buf, lfs_size_t n) {
  memcpy(g_flash + b * BS + o, buf, n); return 0;
}
static int bd_erase(const lfs_config *, lfs_block_t b) { memset(g_flash + b * BS, 0xff, BS); return 0; }
static int bd_sync(const lfs_config *) { return 0; }
static lfs_t g_lfs;
static lfs_config g_cfg;

class File {
 public:
  std::string body;
  bool ok = false;
  void close() {}
};

struct LittleFSClass {
  bool exists(const char *p) { lfs_info info; return lfs_stat(&g_lfs, p, &info) == 0; }
  bool exists(const String &p) { return exists(p.c_str()); }
  File open(const String &p, const char *) {
    File f;
    lfs_file_t lf;
    if (lfs_file_open(&g_lfs, &lf, p.c_str(), LFS_O_RDONLY) == 0) {
      char buf[128]; int n;
      while ((n = lfs_file_read(&g_lfs, &lf, buf, sizeof(buf))) > 0) f.body.append(buf, n);
      lfs_file_close(&g_lfs, &lf);
      f.ok = true;
    }
    return f;
  }
  bool remove(const String &) { return true; }
};
static LittleFSClass LittleFS;

static void put(const char *path, const char *body) {
  lfs_file_t lf;
  lfs_file_open(&g_lfs, &lf, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
  lfs_file_write(&g_lfs, &lf, body, strlen(body));
  lfs_file_close(&g_lfs, &lf);
}

// ---- the web server and auth around handleFile() ----------------------------------------------------------
static int g_status = 0;
static std::string g_served;
struct WebServerStub {
  bool hasArg(const char *) { return false; }
  String arg(const char *) { return String(""); }
  void send_P(int code, PGM_P, PGM_P) { g_status = code; }
  void sendContent(const char *) {}
};
static WebServerStub httpServer;
static const char Helper[] = "helper";
static const char Header[] = "header";
static bool g_passwordSet = true, g_authorised = false;
static bool checkHttpAuth() {
  if (!g_passwordSet || g_authorised) return true;
  g_status = 401;
  return false;
}
static const String &contentType(String &) { static String t("text/plain"); return t; }
static bool streamFileGuarded(File &f, const String &) { g_served = f.body; g_status = 200; return true; }

#include "handlefile.inc"

static const char *SECRET = "httppasswd=s3cret";

// One request as onNotFound makes it: the already-decoded path goes to handleFile(); false means a 404.
static std::string request(const char *decodedPath) {
  g_status = 0; g_served.clear();
  const bool served = handleFile(String(decodedPath));
  if (!served) return "404";
  if (g_status == 401) return "401";
  return g_served == SECRET ? "SETTINGS" : (g_status == 200 ? "200-other" : "?");
}

int main(int argc, char **argv) {
  memset(&g_cfg, 0, sizeof(g_cfg));
  g_cfg.read = bd_read; g_cfg.prog = bd_prog; g_cfg.erase = bd_erase; g_cfg.sync = bd_sync;
  g_cfg.read_size = 16; g_cfg.prog_size = 16; g_cfg.block_size = BS; g_cfg.block_count = BC;
  g_cfg.lookahead_size = 16; g_cfg.cache_size = 64; g_cfg.block_cycles = 500;
  if (lfs_format(&g_lfs, &g_cfg) || lfs_mount(&g_lfs, &g_cfg)) { printf("HARNESS: lfs mount failed\n"); return 2; }
  put("/settings.ini", SECRET);
  put("/FSexplorer.html", "fsx");
  put("/index.html", "idx");
  lfs_mkdir(&g_lfs, "/sat");
  put("/sat/sat.js", "satjs");

  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode == "probe") {   // argv[2..]: decoded paths; print one verdict per path (password set, no credentials)
    g_passwordSet = true; g_authorised = false;
    for (int i = 2; i < argc; i++) printf("%s %s\n", request(argv[i]).c_str(), argv[i]);
    return 0;
  }
  if (mode == "controls") {
    int bad = 0;
    auto expect = [&](const char *label, const char *p, const char *want) {
      const std::string got = request(p);
      printf("%s %-28s %-10s want %s\n", got == want ? "ok  " : "FAIL", label, got.c_str(), want);
      bad += got != want;
    };
    g_passwordSet = true; g_authorised = false;
    expect("no creds /settings.ini", "/settings.ini", "401");
    expect("no creds //settings.ini", "//settings.ini", "401");
    expect("no creds /index.html", "/index.html", "200-other");
    expect("no creds / (index)", "/", "200-other");
    expect("no creds /sat/sat.js", "/sat/sat.js", "200-other");
    expect("no creds /sat/../index.html", "/sat/../index.html", "200-other");
    g_authorised = true;
    expect("authorised /settings.ini", "/settings.ini", "SETTINGS");
    g_passwordSet = false; g_authorised = false;
    expect("no password /settings.ini", "/settings.ini", "SETTINGS");
    return bad ? 1 : 0;
  }
  printf("usage: probe <paths...> | controls\n");
  return 2;
}
