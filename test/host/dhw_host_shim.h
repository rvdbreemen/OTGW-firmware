// test/host/dhw_host_shim.h  (TASK-1123, ADR-176)
//
// Host doubles shared by test_dhw_water_meter.cpp and test_dhw_water_discovery.cpp:
// the Arduino/ESP32 platform surface (PROGMEM macros, the _P string functions,
// millis(), dtostrf, bit macros) and an in-memory LittleFS. Only the platform is
// doubled here; the firmware code under test is sliced from the sources by
// test_dhw_water_meter.py.
#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <math.h>
#include <ctime>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

// ---- PROGMEM domain: one address space on the host --------------------------------
#define PROGMEM
#define PGM_P const char*
#define PSTR(s) (s)
#define FPSTR(p) (p)
class __FlashStringHelper;                                 // pointer-only, as on Arduino
#define F(s) ((const __FlashStringHelper*)(s))
#define snprintf_P snprintf
#define strstr_P strstr
#define strlen_P strlen
#define strcmp_P strcmp
#define strcasecmp_P _stricmp
#define memcpy_P memcpy
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define bitRead(value, bit)  (((value) >> (bit)) & 0x01)
#define bitSet(value, bit)   ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define DebugTf(...)      ((void)0)
#define DebugTln(...)     ((void)0)
#define MQTTDebugTln(...) ((void)0)
typedef uint8_t byte;

// ---- clock ---------------------------------------------------------------------------
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }

static char* dtostrf(double v, signed char width, unsigned char prec, char* buf) {
  std::sprintf(buf, "%*.*f", (int)width, (int)prec, v);
  return buf;
}

// ---- in-memory LittleFS --------------------------------------------------------------
// A file opened with "w" is committed on close(), as LittleFS does; `writes` counts the
// commits, which is what the write-rate rule is about. A path that does not exist gets
// an empty entry at open() (littlefs creates it there; an existing file is truncated
// only by the commit). print() always reports every byte, as the ESP32 VFS does for a
// small write: it lands in a stdio buffer. failNextCommit makes the next close() drop
// the new content without telling the caller (VFSFileImpl::close() ignores fclose()),
// so the file keeps what it had, as on a full filesystem.
class File;
struct FakeFS {
  std::map<std::string, std::string> files;
  int  writes         = 0;
  int  failedCommits  = 0;
  bool failNextCommit = false;
  File open(const char* path, const char* mode);
  bool exists(const char* path) const { return files.count(path) != 0; }
};
static FakeFS LittleFS;
static bool   LittleFSmounted = true;

class File {
 public:
  File() = default;
  File(FakeFS* fs, const std::string& path, bool write) : fs_(fs), path_(path), write_(write) {
    if (!write_) data_ = fs_->files.at(path_);
  }
  explicit operator bool() const { return fs_ != nullptr; }
  size_t print(const char* s) {
    const size_t n = std::strlen(s);
    data_.append(s, n);
    return n;
  }
  size_t readBytes(char* buf, size_t len) {
    const size_t n = std::min(len, data_.size() - pos_);
    std::memcpy(buf, data_.data() + pos_, n);
    pos_ += n;
    return n;
  }
  void close() {
    if (fs_ && write_) {
      if (fs_->failNextCommit) { fs_->failNextCommit = false; fs_->failedCommits++; }
      else                     { fs_->files[path_] = data_; fs_->writes++; }
    }
    fs_ = nullptr;
  }
 private:
  FakeFS*     fs_ = nullptr;
  std::string path_;
  bool        write_ = false;
  std::string data_;
  size_t      pos_ = 0;
};

inline File FakeFS::open(const char* path, const char* mode) {
  const bool write = (mode[0] == 'w');
  if (!write && !files.count(path)) return File();
  if (write && !files.count(path)) files[path] = "";
  return File(this, path, write);
}

// ---- verdict helper -------------------------------------------------------------------
// Flushed per case: a case that ends the process (a /GS stack check) must not take the
// lines of the cases before it along.
static int g_failures = 0;
static bool verdict(const char* id, const char* title, bool ok, const char* got) {
  std::printf("CASE %s %s\n     %s\n     got : %s\n", id, ok ? "pass" : "FAIL", title, got);
  std::fflush(stdout);
  if (!ok) g_failures++;
  return ok;
}
