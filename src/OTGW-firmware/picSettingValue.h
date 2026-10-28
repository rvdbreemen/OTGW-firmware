/*
***************************************************************************
**  Program : picSettingValue.h
**  Version 1.0.0
**
**  Stores a PR= answer from the PIC as text that MQTT, JSON and Home Assistant
**  can read (TASK-1430, GH #688).
**
**  Header-only so it can be tested on the host
**  (test/host/test_picSettingValue.cpp). handlePRresponse() in OTGW-Core.ino
**  is what calls it.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License.
***************************************************************************
*/
#pragma once

#include <stddef.h>
#include <string.h>

// Copies a PR= answer into dst (dstSize bytes, always terminated).
// A printable ASCII answer is copied as is (cut to fit). An answer with any
// other byte, such as the 0xFF that PR=Q returned on GH #688, is stored as
// "0x" plus the hex of every byte, cut at a whole byte; empty when not even
// one byte fits.
static inline void copyPicSettingValue(char* dst, size_t dstSize, const char* src)
{
  if (dstSize == 0) return;
  bool printable = true;
  for (const unsigned char* p = (const unsigned char*)src; *p; p++) {
    if (*p < 0x20 || *p > 0x7E) { printable = false; break; }
  }
  if (printable) {
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
    return;
  }
  dst[0] = '\0';
  if (dstSize < 5) return;  // "0x" + 2 digits + terminator
  size_t n = 0;
  dst[n++] = '0';
  dst[n++] = 'x';
  for (const unsigned char* p = (const unsigned char*)src; *p && n + 2 < dstSize; p++) {
    const unsigned char hi = *p >> 4;
    const unsigned char lo = *p & 0x0F;
    dst[n++] = (char)(hi < 10 ? '0' + hi : 'A' + hi - 10);
    dst[n++] = (char)(lo < 10 ? '0' + lo : 'A' + lo - 10);
  }
  dst[n] = '\0';
}
