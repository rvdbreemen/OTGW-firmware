/*
***************************************************************************
**  Program  : test/host/ha_disc_shim/platform.h
**
**  Stand-in for the Platform library dispatcher <platform.h>, so the real
**  MQTTHaDiscovery.cpp compiles on the host (TASK-1202). The real header
**  pulls in ESP-IDF; the discovery file only reads the free heap from it.
**  The value is a healthy ESP32-S3 heap, so no heap floor ever trips.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#pragma once

#include <cstdint>

inline uint32_t platformFreeHeap()     { return 150000; }
inline uint32_t platformMaxFreeBlock() { return 100000; }
