/*
***************************************************************************
**  Program  : test/host/test_ot_support_migration.cpp
**
**  Host harness for TASK-1185 AC#2: /ot-boiler.json files written by builds
**  that counted answers the gateway made itself as boiler evidence (format 1)
**  must not bring those verdicts back. The maintainer chose a one-time
**  migration: format 2 is the only boiler format that loads; a format-1 file is
**  ignored once, the boiler bitmaps start empty, and the next save writes
**  format 2. The thermostat file (/ot-thermo.json, format 1) is unaffected.
**
**  The code under test is NOT copied here. test_ot_support_migration.py slices
**  it by anchor, verbatim, from OTGW-Core.ino into
**  generated/ot_support_migration/<old|fix>/core.inc: the six support bitmaps,
**  their dirty flags and accessors, and the persistence functions from
**  fileFindToken() through saveOtSupportFilesIfDirty().
**
**  Test doubles: an in-memory LittleFS whose File offers the calls the slices
**  make (read, seek, available, print) and commits a "w" file on close(), as
**  littlefs does; DebugTln is dropped.
**
**  Usage: test_ot_support_migration.exe [dump-dir]
**    With a dump-dir it writes <dump-dir>/cases.txt, one line per case: the
**    boiler and thermostat bitmaps (R), the two file dirty flags (D) and the
**    /ot-boiler.json content after the case (F).
**  Exit 0 when every case holds, 1 otherwise.
**
**  Run: python test/host/test_ot_support_migration.py --old-rev de6b13ebe
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

// ---- platform surface the slices need -------------------------------------------
typedef uint8_t byte;
typedef const char* PGM_P;
#define PROGMEM
#define PSTR(s) (s)
class __FlashStringHelper;                        // incomplete: pointer-only, as on Arduino
#define F(s) ((const __FlashStringHelper*)(s))
#define strstr_P strstr
#define strncpy_P strncpy
#define DebugTln(...) ((void)0)

// ---- in-memory LittleFS -----------------------------------------------------------
struct FakeFS;
class File {
 public:
  File() = default;
  File(FakeFS* fs, const std::string& path, bool write);
  explicit operator bool() const { return fs_ != nullptr; }
  int available() { return (int)(data_.size() - pos_); }
  int read() { return pos_ < data_.size() ? (unsigned char)data_[pos_++] : -1; }
  int read(uint8_t* buf, size_t len) {
    const size_t n = (len < data_.size() - pos_) ? len : data_.size() - pos_;
    std::memcpy(buf, data_.data() + pos_, n);
    pos_ += n;
    return (int)n;
  }
  bool seek(size_t pos) { pos_ = pos <= data_.size() ? pos : data_.size(); return true; }
  size_t print(const char* s) { data_ += s; return std::strlen(s); }
  size_t print(const __FlashStringHelper* s) { return print(reinterpret_cast<const char*>(s)); }
  size_t print(char c) { data_ += c; return 1; }
  size_t print(int v) { std::string s = std::to_string(v); data_ += s; return s.size(); }
  void close();
 private:
  FakeFS* fs_ = nullptr;
  std::string path_;
  bool write_ = false;
  std::string data_;
  size_t pos_ = 0;
};
struct FakeFS {
  std::map<std::string, std::string> files;
  File open(const char* path, const char* mode) {
    const bool write = mode[0] == 'w';
    if (!write && !files.count(path)) return File();
    return File(this, path, write);
  }
  bool remove(const char* path) { return files.erase(path) != 0; }
  bool rename(const char* from, const char* to) {
    auto it = files.find(from);
    if (it == files.end()) return false;
    files[to] = it->second;
    files.erase(it);
    return true;
  }
};
File::File(FakeFS* fs, const std::string& path, bool write) : fs_(fs), path_(path), write_(write) {
  if (!write_) data_ = fs_->files.at(path_);
}
void File::close() {
  if (fs_ && write_) fs_->files[path_] = data_;   // committed on close, as littlefs does
  fs_ = nullptr;
}
static FakeFS LittleFS;

// ---- code under test: OTGW-Core.ino, sliced ----------------------------------------
#include "core.inc"

// ---- harness ------------------------------------------------------------------------
static const char* kBoiler = "/ot-boiler.json";
static const char* kThermo = "/ot-thermo.json";
// A format-1 boiler file as the builds before TASK-1185 wrote it: acked 3 (a cache
// replay), 1 (a master-mode echo), unsupported 27 and 71.
static const char* kBoilerV1 = "{\"v\":1,\"device\":\"boiler\",\"ar\":[3,26],\"aw\":[1],\"ur\":[27],\"uw\":[71]}\n";
static const char* kThermoV1 = "{\"v\":1,\"device\":\"thermostat\",\"sr\":[0,25],\"sw\":[1]}\n";

static void resetRam() {
  memset(boilerLastMasterWasWrite, 0, sizeof(boilerLastMasterWasWrite));
  memset(boilerUnsupportedRead, 0, sizeof(boilerUnsupportedRead));
  memset(boilerUnsupportedWrite, 0, sizeof(boilerUnsupportedWrite));
  memset(boilerAckedRead, 0, sizeof(boilerAckedRead));
  memset(boilerAckedWrite, 0, sizeof(boilerAckedWrite));
  memset(thermostatSentRead, 0, sizeof(thermostatSentRead));
  memset(thermostatSentWrite, 0, sizeof(thermostatSentWrite));
  boilerUnsupportedDirty = boilerFileDirty = thermostatFileDirty = false;
}
static void resetAll() {
  resetRam();
  LittleFS.files.clear();
}
static int bits(const uint8_t* b) {
  int n = 0;
  for (int i = 0; i < 32; i++) for (int k = 0; k < 8; k++) n += (b[i] >> k) & 1;
  return n;
}
static int boilerBits() {
  return bits(boilerAckedRead) + bits(boilerAckedWrite) + bits(boilerUnsupportedRead) + bits(boilerUnsupportedWrite);
}
static std::string hex(const uint8_t* bitmap) {
  std::string s;
  char b[4];
  for (int i = 0; i < 32; i++) { snprintf(b, sizeof(b), "%02X", bitmap[i]); s += b; }
  return s;
}
static std::string oneLine(std::string s) {
  for (char& c : s) if (c == '\n' || c == ' ') c = '_';
  return s.empty() ? "-" : s;
}

static FILE* g_dump = nullptr;
static int g_failures = 0;
static void finish(const char* id, const char* title, bool ok, const char* got) {
  if (!ok) g_failures++;
  std::printf("CASE %s %s\n     %s\n     got : %s\n", id, ok ? "pass" : "FAIL", title, got);
  if (g_dump) {
    const auto it = LittleFS.files.find(kBoiler);
    std::fprintf(g_dump, "%s R=ar:%s,aw:%s,ur:%s,uw:%s,sr:%s,sw:%s D=bf:%d,tf:%d F=%s\n", id,
                 hex(boilerAckedRead).c_str(), hex(boilerAckedWrite).c_str(), hex(boilerUnsupportedRead).c_str(),
                 hex(boilerUnsupportedWrite).c_str(), hex(thermostatSentRead).c_str(), hex(thermostatSentWrite).c_str(),
                 (int)boilerFileDirty, (int)thermostatFileDirty,
                 oneLine(it == LittleFS.files.end() ? std::string() : it->second).c_str());
  }
}

int main(int argc, char** argv) {
  if (argc > 1) {
    std::string path = std::string(argv[1]) + "/cases.txt";
    g_dump = std::fopen(path.c_str(), "w");
  }
  char got[256];

  std::printf("== migration of a format-1 /ot-boiler.json ==\n");
  resetAll();
  LittleFS.files[kBoiler] = kBoilerV1;
  loadOtSupportFiles();
  snprintf(got, sizeof got, "boiler bits loaded=%d, boilerFileDirty=%d", boilerBits(), (int)boilerFileDirty);
  finish("M1", "boot with a format-1 boiler file: its verdicts stay out of RAM, and the file is due to be rewritten",
         boilerBits() == 0 && boilerFileDirty, got);

  resetAll();
  LittleFS.files[kBoiler] = kBoilerV1;
  loadOtSupportFiles();
  saveOtSupportFilesIfDirty();
  {
    const std::string f = LittleFS.files.count(kBoiler) ? LittleFS.files[kBoiler] : "";
    const bool v2 = f.rfind("{\"v\":2,\"device\":\"boiler\"", 0) == 0;
    const bool empty = f.find("\"ar\":[]") != std::string::npos && f.find("\"aw\":[]") != std::string::npos &&
                       f.find("\"ur\":[]") != std::string::npos && f.find("\"uw\":[]") != std::string::npos;
    snprintf(got, sizeof got, "file after the save: %s", oneLine(f).c_str());
    finish("M2", "the first save after that writes format 2 with empty boiler arrays", v2 && empty, got);
  }

  resetAll();
  LittleFS.files[kBoiler] = "{\"v\":2,\"device\":\"boiler\",\"ar\":[3,26],\"aw\":[1],\"ur\":[27],\"uw\":[71]}\n";
  loadOtSupportFiles();
  snprintf(got, sizeof got, "boiler bits loaded=%d (want 5), boilerFileDirty=%d", boilerBits(), (int)boilerFileDirty);
  finish("M3", "a format-2 boiler file loads: acked 3, 26 and 1, unsupported 27 and 71",
         boilerBits() == 5 && isBoilerMsgIdAckedRead(3) && isBoilerMsgIdAckedRead(26) && isBoilerMsgIdAckedWrite(1) &&
         isBoilerMsgIdUnsupportedRead(27) && isBoilerMsgIdUnsupportedWrite(71) && !boilerFileDirty, got);

  std::printf("== controls ==\n");
  resetAll();
  boilerAckedRead[25 >> 3] |= 1u << (25 & 7);
  boilerUnsupportedWrite[71 >> 3] |= 1u << (71 & 7);
  boilerFileDirty = true;
  saveOtSupportFilesIfDirty();
  resetRam();
  loadOtSupportFiles();
  snprintf(got, sizeof got, "after save and reload: acked read 25=%d, unsupported write 71=%d, bits=%d, dirty=%d",
           (int)isBoilerMsgIdAckedRead(25), (int)isBoilerMsgIdUnsupportedWrite(71), boilerBits(), (int)boilerFileDirty);
  finish("C1", "a boiler file this build writes loads back unchanged on the next boot",
         isBoilerMsgIdAckedRead(25) && isBoilerMsgIdUnsupportedWrite(71) && boilerBits() == 2 && !boilerFileDirty, got);

  resetAll();
  LittleFS.files[kThermo] = kThermoV1;
  loadOtSupportFiles();
  snprintf(got, sizeof got, "thermostat sent read 0/25=%d/%d, write 1=%d, thermostatFileDirty=%d",
           (int)isThermostatMsgIdSentRead(0), (int)isThermostatMsgIdSentRead(25), (int)isThermostatMsgIdSentWrite(1),
           (int)thermostatFileDirty);
  finish("C2", "the thermostat file (format 1) still loads",
         isThermostatMsgIdSentRead(0) && isThermostatMsgIdSentRead(25) && isThermostatMsgIdSentWrite(1) &&
         !thermostatFileDirty, got);

  resetAll();
  loadOtSupportFiles();
  snprintf(got, sizeof got, "bits=%d, boilerFileDirty=%d, file present=%d", boilerBits(), (int)boilerFileDirty,
           (int)LittleFS.files.count(kBoiler));
  finish("C3", "no boiler file: start empty and write nothing", boilerBits() == 0 && !boilerFileDirty, got);

  resetAll();
  LittleFS.files[kBoiler] = "garbage without a header\n";
  loadOtSupportFiles();
  snprintf(got, sizeof got, "bits=%d, boilerFileDirty=%d", boilerBits(), (int)boilerFileDirty);
  finish("C4", "a boiler file without a valid header: start empty, as before", boilerBits() == 0 && !boilerFileDirty, got);

  if (g_dump) std::fclose(g_dump);
  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
