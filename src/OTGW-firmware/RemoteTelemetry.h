/*
***************************************************************************
**  Program  : RemoteTelemetry.h
**  TASK-2901 proof of concept (branch research/cloud-recording only): the capture side of the cloud
**  recorder. debugStuff.h routes Debug() / Debugln() through remoteTelemetryTee when HAS_REMOTE_TELEMETRY is 1, so every
**  byte still goes to telnet and the line is also recorded, with each macro argument evaluated once.
**  The recorder, the encoders and the upload task live in RemoteTelemetry.ino and RemoteTelemetryCodec.h.
***************************************************************************
*/
#pragma once

#include <Arduino.h>
#include <stdarg.h>

#if HAS_REMOTE_TELEMETRY

// Capture points (all non-blocking: when the recorder is busy the item is counted as dropped).
void remoteTelemetryBol(const char *fn, int line, int64_t tUs, uint32_t heap, uint32_t block);   // _debugBOL
void remoteTelemetryPrintf(const char *fmt, va_list ap, const char *printed, size_t printedLen, bool truncated);   // _debugPrintf_P
void remoteTelemetryOtFrame(char letter, uint32_t frame);                                       // processOT()
void remoteTelemetryTeeFlush();                                                                  // end of a tee line
void remoteTelemetrySetup();                                                                     // from setup()
void remoteTelemetryInstallTlsAllocator();   // first line of setup(): mbedTLS blocks above 4 KB to PSRAM (TASK-2901)
void remoteTelemetrySatTickf(const char *fmt, ...);   // a SATTICK line with SAT debug off: recorder only (TASK-2905)

// TASK-2907: local recording into a ring of the same records (the /api/v2/diag/recording routes in this env).
#include "RecRing.h"
struct RemoteTelemetryLocalStatus {
  bool active, psram, exact, hasData;
  uint8_t lastStart;                 // a DiagStartResult
  uint32_t entries, capBytes, dropped;
  uint64_t oldest, newest;
};
uint8_t remoteTelemetryLocalStart();        // a DiagStartResult
bool remoteTelemetryLocalStop();
void remoteTelemetryLocalStatusRead(RemoteTelemetryLocalStatus &st);
size_t remoteTelemetryLocalFill(rtel::RingCursor &c, uint8_t *buf, size_t cap);   // DIAG_FILL_TRY_AGAIN when busy, 0 at the end
const char *remoteTelemetryUidText();

// Print that writes through to the telnet debug sink and keeps a copy of the current line.
class RemoteTelemetryTee : public Print {
 public:
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t size) override;
};
extern RemoteTelemetryTee remoteTelemetryTee;

#endif
