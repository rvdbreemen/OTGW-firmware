//=======================================================================
// arduino_shim.h — minimal host-side emulation of the Arduino/ESP8266
// platform, just enough to compile the REAL src/OTGW-firmware/jsonStuff.ino
// on a desktop compiler so extractJsonField() can be unit-tested.
//
// This file emulates the PLATFORM. It never reimplements any code under
// test: jsonStuff.ino is #included verbatim by the test translation unit.
//=======================================================================
#pragma once

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <string>

//---------------------------------------------------------------- PROGMEM
// On the host everything lives in one address space, so the PROGMEM
// qualifiers collapse to nothing and the _P helpers to their plain twins.
#define PROGMEM
typedef const char* PGM_P;

class __FlashStringHelper;
#define PSTR(s) (s)
#define F(s)    (reinterpret_cast<const __FlashStringHelper*>(PSTR(s)))

#define strncpy_P   strncpy
#define strcmp_P    strcmp
#define strcasecmp_P _stricmp
#define memcmp_P    memcmp
#define strstr_P    strstr
#define pgm_read_char(p) (*(const char*)(p))

// snprintf_P must NOT be a plain alias for snprintf: the ESP8266 core adds a
// '%S' (capital S) conversion meaning "PROGMEM char*". Host glibc/MSVC read
// '%S' as wchar_t*, which would silently produce garbage. Rewrite %S -> %s
// (leaving %%S alone) before handing the format to vsnprintf.
inline int snprintf_P(char* dst, size_t size, const char* fmt, ...) {
  std::string f;
  for (const char* p = fmt; *p; ++p) {
    if (*p == '%' && *(p + 1) == '%') { f += "%%"; ++p; continue; }
    if (*p == '%' && *(p + 1) == 'S') { f += "%s"; ++p; continue; }
    f += *p;
  }
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(dst, size, f.c_str(), ap);
  va_end(ap);
  return n;
}

//---------------------------------------------------------------- String
// Thin stand-in for the Arduino String class; only the members jsonStuff.ino
// actually touches are provided.
class String {
public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
private:
  std::string s_;
};

//---------------------------------------------------------------- File
// jsonStuff.ino's readJsonStringPair()/writeJsonStringPair() need a File type
// to compile. They are not exercised by these tests, so the stub is inert.
class File {
public:
  int  read() { return -1; }
  void print(char) {}
  void print(const char*) {}
  void print(const __FlashStringHelper*) {}
};

//---------------------------------------------------------------- httpServer
#define CONTENT_LENGTH_UNKNOWN 0xFFFFFFFFU

class HostWebServerStub {
public:
  void sendHeader(const __FlashStringHelper*, const __FlashStringHelper*) {}
  void setContentLength(size_t) {}
  void send_P(int, PGM_P, PGM_P) {}
  void sendContent(const char*, size_t) {}
};
static HostWebServerStub httpServer;

//---------------------------------------------------------------- misc glue
#define DebugTf(...)  do { } while (0)
#define DebugTln(...) do { } while (0)

// Mirrors OTGW-firmware.h
#define CMSG_SIZE       512
#define JSON_ENTRY_BUF  256
#define CBOOLEAN(x) ((x) ? "true" : "false")
inline const char* CSTR(const String& x) { return x.c_str(); }
inline const char* CSTR(const char* x)   { return x ? x : ""; }
inline const char* CSTR(char* x)         { return x ? x : ""; }

// The global scratch buffer extractJsonField() borrows for its search pattern.
char cMsg[CMSG_SIZE];

// The Arduino build generates prototypes for every .ino function; a plain C++
// TU does not, and jsonStuff.ino calls jsonBufAppend() before defining it.
static void jsonBufAppend(const char* s, size_t len);
