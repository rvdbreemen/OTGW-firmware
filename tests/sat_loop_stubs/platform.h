// Host stand-in for src/libraries/Platform/src/platform.h: only what the SAT type headers need. The real platform shims (mutex, queue,
// task, PSRAM) are not modelled; the few the SAT code calls are in sat_loop_stubs.h.
#pragma once
#include <Arduino.h>
static inline uint32_t platformHardwareRandom() { return (uint32_t)rand(); }   // STUB: pseudo-random, not the ESP32 hardware RNG
