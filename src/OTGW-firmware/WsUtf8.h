/*
***************************************************************************
**  Program  : WsUtf8.h
**  Version  : v2.0.0-alpha.551
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Pure UTF-8 helpers for the WebSocket text path (TASK-1300). A browser
**  closes the socket when a text frame is not valid UTF-8, and raw PIC
**  bytes (0x80-0xFF) or a log line cut inside a multi-byte sequence end up
**  in such frames. The check follows the WHATWG/Unicode rules Chrome
**  applies: no overlong forms, no surrogates, nothing above U+10FFFF.
**  No Arduino or firmware state in here, so a host test compiles this
**  header as it is.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Length of the valid UTF-8 sequence starting at s[0], or 0 when the bytes
// at s do not start one. A NUL byte ends the string, so a sequence cut by
// the terminator is invalid (0).
static inline size_t wsUtf8SeqLen(const char* s)
{
  const uint8_t b0 = (uint8_t)s[0];
  if (b0 < 0x80) return b0 ? 1 : 0;
  uint8_t lo = 0x80, hi = 0xBF;   // allowed range of the second byte
  size_t n;
  if      (b0 >= 0xC2 && b0 <= 0xDF) { n = 2; }
  else if (b0 == 0xE0)               { n = 3; lo = 0xA0; }
  else if (b0 == 0xED)               { n = 3; hi = 0x9F; }
  else if (b0 >= 0xE1 && b0 <= 0xEF) { n = 3; }
  else if (b0 == 0xF0)               { n = 4; lo = 0x90; }
  else if (b0 == 0xF4)               { n = 4; hi = 0x8F; }
  else if (b0 >= 0xF1 && b0 <= 0xF3) { n = 4; }
  else return 0;                  // 80-BF as lead, C0, C1, F5-FF
  const uint8_t b1 = (uint8_t)s[1];
  if (b1 < lo || b1 > hi) return 0;
  for (size_t i = 2; i < n; i++) {
    const uint8_t b = (uint8_t)s[i];
    if (b < 0x80 || b > 0xBF) return 0;
  }
  return n;
}

// True when the whole NUL-terminated string is valid UTF-8.
static inline bool wsUtf8Valid(const char* s)
{
  if (s == nullptr) return true;
  while (*s) {
    const size_t n = wsUtf8SeqLen(s);
    if (n == 0) return false;
    s += n;
  }
  return true;
}

// Copy in to out (outSize bytes, always NUL-terminated). Valid sequences are
// copied unchanged, each byte that cannot start or complete a valid sequence
// becomes '?', and an incomplete sequence at the end of the string is
// dropped. The output is never longer than the input. Returns the output length.
static inline size_t wsUtf8Sanitize(const char* in, char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0) return 0;
  size_t o = 0;
  if (in != nullptr) {
    while (*in) {
      const size_t n = wsUtf8SeqLen(in);
      if (n > 0) {
        if (o + n > outSize - 1) break;
        memcpy(out + o, in, n);
        o += n;
        in += n;
        continue;
      }
      // Not a valid sequence here. A lead byte whose sequence runs into the
      // end of the string is an incomplete tail: drop it.
      const uint8_t b0 = (uint8_t)*in;
      size_t need = 0;
      if      (b0 >= 0xC2 && b0 <= 0xDF) need = 2;
      else if (b0 >= 0xE0 && b0 <= 0xEF) need = 3;
      else if (b0 >= 0xF0 && b0 <= 0xF4) need = 4;
      if (need > 0) {
        size_t have = 1;
        while (have < need && in[have] && ((uint8_t)in[have] & 0xC0) == 0x80) have++;
        if (in[have] == '\0' && have < need) {
          // Dropped only when the partial sequence is still a valid prefix.
          bool prefixOk = true;
          if (have >= 2) {
            const uint8_t b1 = (uint8_t)in[1];
            if ((b0 == 0xE0 && b1 < 0xA0) || (b0 == 0xED && b1 > 0x9F) ||
                (b0 == 0xF0 && b1 < 0x90) || (b0 == 0xF4 && b1 > 0x8F)) prefixOk = false;
          }
          if (prefixOk) break;
        }
      }
      if (o + 1 > outSize - 1) break;
      out[o++] = '?';
      in++;
    }
  }
  out[o] = '\0';
  return o;
}
