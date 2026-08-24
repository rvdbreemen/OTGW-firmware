/*
***************************************************************************
**  Program  : test/host/arduino_shim.h
**
**  Minimal host-side emulation of the Arduino/ESP32 platform surface that
**  the code under test touches. This file emulates the PLATFORM only: the
**  functions being tested (extractJsonField, expandPayload) are never
**  reimplemented here - they are extracted verbatim from the shipped
**  sources by build_and_run.ps1 (see the sentinel comments in
**  src/OTGW-firmware/jsonStuff.ino and src/OTGW-firmware/webhook.ino).
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#pragma once

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <string>

// ---- PROGMEM domain --------------------------------------------------------
// On the host there is only one address space, so the flash/RAM distinction
// collapses to plain pointers. The _P helpers keep their names so the source
// under test compiles unmodified.
#define PROGMEM
#define PSTR(s)            (s)
typedef const char*        PGM_P;
class __FlashStringHelper;                       // incomplete: pointer-only, as on Arduino
#define F(s)               ((const __FlashStringHelper*)(s))

#define snprintf_P         snprintf
#define strcmp_P           strcmp
#define strncmp_P          strncmp
#define strcasecmp_P       _stricmp
#define memcmp_P           memcmp
#define strstr_P           strstr
#define strlen_P           strlen

static inline size_t hostStrlcpy(char* dst, const char* src, size_t size) {
  size_t sl = strlen(src);
  if (size) {
    size_t n = (sl >= size) ? size - 1 : sl;
    memcpy(dst, src, n);
    dst[n] = '\0';
  }
  return sl;
}
#define strlcpy            hostStrlcpy
#define strlcpy_P          hostStrlcpy
static inline char* hostStrncpyP(char* d, const char* s, size_t n) { return strncpy(d, s, n); }
#define strncpy_P          hostStrncpyP

// ---- Arduino String (only c_str() is exercised by the code under test) -----
class String {
public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  const char* c_str() const { return s_.c_str(); }
private:
  std::string s_;
};

// ---- OpenTherm state surface used by expandPayload -------------------------
// Field names and initialisers mirror OTGW-Core.h (Tr = NAN per TASK-522);
// only the members expandPayload reads are present.
struct HostOTState {
  float    Tboiler     = 0.0f;
  float    Tr          = NAN;
  float    TSet        = 0.0f;
  float    Tdhw        = 0.0f;
  float    RelModLevel = 0.0f;
  float    CHPressure  = 0.0f;
  uint16_t SlaveStatus = 0;
};
extern HostOTState OTcurrentSystemState;
