// Host-test stub for <Arduino.h>: only what src/OTGW-firmware/jsonEmit.h and
// graphHistoryStuff.h use (Print, F(), PSTR, PGM_P, FPSTR, snprintf_P, String).
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define PROGMEM
typedef const char* PGM_P;
#define FPSTR(p) (reinterpret_cast<const __FlashStringHelper*>(p))
#define snprintf_P snprintf

class String {
public:
  String(const char* s = "") : _s(s) {}
  const char* c_str() const { return _s.c_str(); }
private:
  std::string _s;
};

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t b) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) {
    for (size_t i = 0; i < size; ++i) write(buf[i]);
    return size;
  }
  size_t write(const char* s) { return s ? write((const uint8_t*)s, strlen(s)) : 0; }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(const char* s) { return write(s); }
  size_t print(const __FlashStringHelper* s) { return write(reinterpret_cast<const char*>(s)); }
};
