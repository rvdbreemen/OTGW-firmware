//=======================================================================
// IPAddress.h - host-side stub for PubSubClient. Platform emulation only.
//=======================================================================
#pragma once

#include <cstdint>

class IPAddress {
public:
  IPAddress() : _a(0), _b(0), _c(0), _d(0) {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : _a(a), _b(b), _c(c), _d(d) {}
  uint8_t operator[](int i) const {
    switch (i) { case 0: return _a; case 1: return _b; case 2: return _c; default: return _d; }
  }
private:
  uint8_t _a, _b, _c, _d;
};
