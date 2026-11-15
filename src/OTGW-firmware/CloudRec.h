/*
***************************************************************************
**  Program  : CloudRec.h
**  TASK-1963 proof of concept (branch research/cloud-recording only): the capture side of the cloud
**  recorder. debugStuff.h routes Debug() / Debugln() through cloudRecTee when HAS_CLOUD_REC is 1, so every
**  byte still goes to telnet and the line is also recorded, with each macro argument evaluated once.
**  The recorder, the encoders and the upload task live in CloudRec.ino and CloudRecCodec.h.
***************************************************************************
*/
#pragma once

#include <Arduino.h>
#include <stdarg.h>

#if HAS_CLOUD_REC

// Capture points (all non-blocking: when the recorder is busy the item is counted as dropped).
void cloudRecBol(const char *fn, int line, int64_t tUs, uint32_t heap, uint32_t block);   // _debugBOL
void cloudRecPrintf(const char *fmt, va_list ap, const char *printed, size_t printedLen, bool truncated);   // _debugPrintf_P
void cloudRecOtFrame(char letter, uint32_t frame);                                       // processOT()
void cloudRecTeeFlush();                                                                  // end of a tee line
void cloudRecSetup();                                                                     // from setup()
void cloudRecSatTickf(const char *fmt, ...);   // a SATTICK line with SAT debug off: recorder only (TASK-1974)

// TASK-1991: local recording into a ring of the same records (the /api/v2/diag/recording routes in this env).
#include "RecRing.h"
struct CloudRecLocalStatus {
  bool active, psram, exact, hasData;
  uint8_t lastStart;                 // a DiagStartResult
  uint32_t entries, capBytes, dropped;
  uint64_t oldest, newest;
};
uint8_t cloudRecLocalStart();        // a DiagStartResult
bool cloudRecLocalStop();
void cloudRecLocalStatusRead(CloudRecLocalStatus &st);
size_t cloudRecLocalFill(crec::RingCursor &c, uint8_t *buf, size_t cap);   // DIAG_FILL_TRY_AGAIN when busy, 0 at the end
const char *cloudRecUidText();

// Print that writes through to the telnet debug sink and keeps a copy of the current line.
class CloudRecTee : public Print {
 public:
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t size) override;
};
extern CloudRecTee cloudRecTee;

#endif
