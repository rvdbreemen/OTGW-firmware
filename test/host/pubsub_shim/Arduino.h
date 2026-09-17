//=======================================================================
// Arduino.h - host-side platform stub, just enough to compile the REAL
// libraries/PubSubClient/src/PubSubClient.cpp on a desktop compiler.
//
// This emulates the PLATFORM only. PubSubClient itself is compiled
// verbatim; nothing it does is reimplemented here.
//=======================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <cstddef>

typedef uint8_t byte;
typedef bool boolean;

// On the host everything lives in one address space, so the PROGMEM
// accessors collapse to plain dereferences.
#define pgm_read_byte_near(p) (*(const uint8_t*)(p))

// Host-side clock the test drives directly, so a socketTimeout can be
// reached without the test actually sleeping.
extern unsigned long g_hostMillis;
inline unsigned long millis() { return g_hostMillis; }
inline void yield() {}

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) {
    size_t n = 0;
    while (size--) {
      if (write(*buf++) != 1) break;
      n++;
    }
    return n;
  }
};
