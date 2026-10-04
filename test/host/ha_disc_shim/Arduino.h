/*
***************************************************************************
**  Program  : test/host/ha_disc_shim/Arduino.h
**
**  Stand-in for <Arduino.h> so test_ha_discovery_json.py can compile the
**  real MQTTHaDiscovery.cpp (which includes <Arduino.h>) and the slices of
**  the firmware sources on the host (TASK-1202). Platform surface only: the
**  C headers the ESP32 core pulls in, F(), byte, the bit macros and dtostrf.
**  Nothing of the discovery code is reimplemented here.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pgmspace.h"

class __FlashStringHelper;                  // pointer-only, as on Arduino
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(PSTR(s)))

typedef uint8_t byte;

#define bitRead(value, bit)  (((value) >> (bit)) & 0x01)
#define bitSet(value, bit)   ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))

// The core's dtostrf (stdlib_noniso.c) renders the values the firmware passes
// to it here (whole numbers and one or two decimals) as "%*.*f" does.
inline char *dtostrf(double val, signed int width, unsigned int prec, char *s) {
  std::sprintf(s, "%*.*f", width, (int)prec, val);
  return s;
}
