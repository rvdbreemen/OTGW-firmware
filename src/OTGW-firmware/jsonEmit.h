/*
***************************************************************************
**  Program  : jsonEmit.h
**  Version  : v2.0.0-alpha.231
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Embedded-robust streaming JSON writer (TASK-885).
**
**  Emits type-correct JSON DIRECTLY into an Arduino Print sink (the
**  AsyncResponseStream), with NO intermediate in-memory document tree and NO
**  whole-response buffer that the writer itself owns. Design principle: embedded
**  robustness over throughput.
**
**    - NON-THROWING. Every out.print()/out.write() return value is discarded;
**      the writer never calls new, never grows a String in the hot path, never
**      throws. A full sink truncates the wire; the firmware never crashes and
**      the writer's bookkeeping (depth/comma state) stays balanced regardless
**      of how many bytes the sink accepted. This is the property the pre-
**      migration hand-rolled path had that let alpha.204 survive a 16-worker
**      flood where the ArduinoJson document path rebooted (TASK-883/884).
**    - NO MATERIALIZED DOCUMENT. State is read and emitted field-by-field.
**    - MEDIUM-CHUNK WRITES. One out.print(...) per field / per escaped segment
**      (tens..~1460 bytes), never byte-at-a-time. Byte-at-a-time into a growing
**      cbuf was the ~8600x "Failed to allocate" log storm that starved
**      async_tcp past the 30 s watchdog (alpha.211); medium chunks cap it.
**
**  WIRING: single-pass into one AsyncResponseStream per response (see
**  restSendJsonStream in webServerCompat.h). Chunked-re-run is deliberately NOT
**  used here: a no-document producer re-reads live volatile state (uptime,
**  freeheap) on each chunk pass, so a field's decimal width can shift between
**  passes and desynchronize the byte window -> corrupt JSON. Single-pass emits
**  every volatile field exactly once. (The ArduinoJson chunked path in ADR-145
**  is safe only because it replays an immutable document snapshot.)
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef JSONEMIT_H
#define JSONEMIT_H

#include <Arduino.h>
#include <math.h>     // isnan / isinf
#include <platform.h> // PLATFORM_INT_DISTINCT_FROM_INT32 must be defined before the int/unsigned field overloads below are parsed

// PLATFORM_INT_DISTINCT_FROM_INT32 mirrors the guard the old sendJsonMapEntry
// layer used (jsonStuff.ino): on this ESP32-S3 toolchain int32_t IS int, so the
// macro is left undefined (= 0 in #if) and the int/unsigned overloads below are
// excluded to avoid redefining the int32_t/uint32_t ones. The narrow int8/16/
// uint16 overloads are always present so narrow-int arguments bind by exact
// match (no signed/unsigned promotion ambiguity).

// Escape a NUL-terminated C string into JSON per RFC 8259 and write it to the
// sink in medium chunks (24-byte accumulate-then-flush). Non-throwing: only
// out.print() is used. Byte-for-byte the old escape set (0a3e5eee jsonStuff.ino
// :26-103) with ONE correction: the control-char test is on (unsigned char) so
// bytes 0x80..0xFF (UTF-8 lead/continuation) pass through raw instead of being
// mis-escaped as \u00XX (the old signed `*p < 0x20` test diverged from
// ArduinoJson on any non-ASCII string when char is signed on Xtensa).
// SAFETY: this walks a char* terminated on '\0'. Never point it at a non-NUL-
// terminated binary buffer (project rule: binary uses memcmp_P only).
inline void jsonEscapeTo(Print& out, const char* src) {
  if (!src) return;
  char chunk[24];
  size_t chunkIdx = 0;
  for (const char* p = src; *p; ++p) {
    const char* esc = nullptr;
    char hex[7];
    switch (*p) {
      case '"':  esc = "\\\""; break;
      case '\\': esc = "\\\\"; break;
      case '\b': esc = "\\b";  break;
      case '\f': esc = "\\f";  break;
      case '\n': esc = "\\n";  break;
      case '\r': esc = "\\r";  break;
      case '\t': esc = "\\t";  break;
      default:
        if ((unsigned char)*p < 0x20) {
          snprintf_P(hex, sizeof(hex), PSTR("\\u%04X"), (unsigned char)*p);
          esc = hex;
        }
        break;
    }
    if (esc) {
      const size_t escLen = strlen(esc);
      if (chunkIdx + escLen >= sizeof(chunk)) { chunk[chunkIdx] = '\0'; out.print(chunk); chunkIdx = 0; }
      memcpy(chunk + chunkIdx, esc, escLen);
      chunkIdx += escLen;
    } else {
      if (chunkIdx + 1 >= sizeof(chunk)) { chunk[chunkIdx] = '\0'; out.print(chunk); chunkIdx = 0; }
      chunk[chunkIdx++] = *p;   // raw byte incl. 0x80..0xFF
    }
  }
  if (chunkIdx > 0) { chunk[chunkIdx] = '\0'; out.print(chunk); }
}

class JsonEmit {
public:
  static constexpr uint8_t MAX_DEPTH = 8;

  explicit JsonEmit(Print& out)
    : _out(out), _depth(0), _firstMask(0), _suppressSep(false), _bDepthError(false), _dropped(0) {}

  // ---- structure ----
  void beginObject()                            { _openContainer('{'); }
  void beginObject(const char* k)               { if (_depth >= MAX_DEPTH) { _bDepthError = true; _dropped++; return; } key(k); _openContainer('{'); }
  void beginObject(const __FlashStringHelper* k){ if (_depth >= MAX_DEPTH) { _bDepthError = true; _dropped++; return; } key(k); _openContainer('{'); }
  void endObject()                              { _closeContainer('}'); }
  void beginArray()                             { _openContainer('['); }
  void beginArray(const char* k)                { if (_depth >= MAX_DEPTH) { _bDepthError = true; _dropped++; return; } key(k); _openContainer('['); }
  void beginArray(const __FlashStringHelper* k) { if (_depth >= MAX_DEPTH) { _bDepthError = true; _dropped++; return; } key(k); _openContainer('['); }
  void endArray()                               { _closeContainer(']'); }

  // ---- keys ----
  // Runtime key (escaped): dynamic keys like area_%u_* and Dallas String(addr).
  void key(const char* k) {
    _sep();
    _out.print('"');
    jsonEscapeTo(_out, k);
    _out.print(F("\":"));
    _suppressSep = true;   // the value/container that follows is key-attached
  }
  // Compile-time PROGMEM key F("..."): an author-controlled identifier, no escape
  // needed; Print::print(const __FlashStringHelper*) reads PROGMEM correctly.
  void key(const __FlashStringHelper* k) {
    _sep();
    _out.print('"');
    _out.print(k);
    _out.print(F("\":"));
    _suppressSep = true;
  }

  // ---- scalar values ----
  void value(bool b)     { _sep(); _out.print(b ? F("true") : F("false")); }
  void value(int32_t v)  { _sep(); char b[12]; snprintf_P(b, sizeof(b), PSTR("%ld"), (long)v);          _out.print(b); }
  void value(uint32_t v) { _sep(); char b[12]; snprintf_P(b, sizeof(b), PSTR("%lu"), (unsigned long)v); _out.print(b); }
  // Emits the float as a JSON number with up to `decimals` fractional digits, then
  // trims trailing zeros (and a lone trailing '.') so the wire form matches
  // ArduinoJson's natural representation: 0.000500 -> 0.0005, 21.500000 -> 21.5,
  // 21.000000 -> 21. The default of 6 digits is wide enough to preserve sub-0.001
  // control gains (e.g. an integral gain of 0.0005) that a fixed %.3f silently
  // rounded to 0.001 (TASK-886 review: the OTD kp/ki/... contract). Pass a smaller
  // `decimals` to cap precision deliberately. NaN/Inf -> null (no JSON form).
  void value(float f, uint8_t decimals = 6) {
    _sep();
    if (isnan(f) || isinf(f)) { _out.print(F("null")); return; }
    char fmt[8]; snprintf_P(fmt, sizeof(fmt), PSTR("%%.%uf"), (unsigned)decimals);
    char b[24];  snprintf(b, sizeof(b), fmt, (double)f);
    char* dot = strchr(b, '.');
    if (dot) {
      char* end = b + strlen(b) - 1;
      while (end > dot && *end == '0') *end-- = '\0';                 // strip trailing zeros
      if (end == dot) *end = '\0';                                    // strip lone trailing '.'
      if (b[0] == '-' && b[1] == '0' && b[2] == '\0') { b[0] = '0'; b[1] = '\0'; }  // -0 -> 0
    }
    _out.print(b);
  }
  void value(const char* s) {
    _sep();
    if (!s) { _out.print(F("null")); return; }
    _out.print('"'); jsonEscapeTo(_out, s); _out.print('"');
  }
  void value(const String& s) { value(s.c_str()); }
  // Compile-time PROGMEM value F("..."): escaped for safety. The
  // reinterpret_cast is valid here because the 2.0.0 line is ESP32-S3-only
  // (ADR-128) and the S3 maps flash into the data address space, so a
  // __FlashStringHelper* is directly readable as a const char*.
  void value(const __FlashStringHelper* s) {
    _sep();
    if (!s) { _out.print(F("null")); return; }
    _out.print('"'); jsonEscapeTo(_out, reinterpret_cast<const char*>(s)); _out.print('"');
  }

  // narrow-int forwarders (exact-match to dodge promotion ambiguity)
  void value(int8_t v)   { value((int32_t)v); }
  void value(int16_t v)  { value((int32_t)v); }
  void value(uint16_t v) { value((uint32_t)v); }
#if PLATFORM_INT_DISTINCT_FROM_INT32
  void value(int v)          { value((int32_t)v); }
  void value(unsigned int v) { value((uint32_t)v); }
#endif

  // raw pre-formatted JSON token (e.g. settings' addNum emits an unquoted number)
  void raw(const char* json) { _sep(); _out.print(json); }
  void rawP(PGM_P json)      { _sep(); _out.print(FPSTR(json)); }

  // ---- key+value conveniences ----
  // Templated on the key type K (const char* OR const __FlashStringHelper*, both
  // have a key() overload); the value type resolves through the value() overloads
  // above (including the narrow-int and PROGMEM-string ones). One float overload
  // carries the optional decimals argument.
  template<typename K> void field(K k, bool b)                          { key(k); value(b); }
  template<typename K> void field(K k, int32_t v)                       { key(k); value(v); }
  template<typename K> void field(K k, uint32_t v)                      { key(k); value(v); }
  template<typename K> void field(K k, int8_t v)                        { key(k); value((int32_t)v); }
  template<typename K> void field(K k, int16_t v)                       { key(k); value((int32_t)v); }
  template<typename K> void field(K k, uint16_t v)                      { key(k); value((uint32_t)v); }
  template<typename K> void field(K k, const char* s)                  { key(k); value(s); }
  template<typename K> void field(K k, const String& s)               { key(k); value(s.c_str()); }
  template<typename K> void field(K k, const __FlashStringHelper* s)   { key(k); value(s); }
  template<typename K> void field(K k, float f, uint8_t dec = 6)        { key(k); value(f, dec); }
#if PLATFORM_INT_DISTINCT_FROM_INT32
  template<typename K> void field(K k, int v)                          { key(k); value((int32_t)v); }
  template<typename K> void field(K k, unsigned int v)                 { key(k); value((uint32_t)v); }
#endif
  template<typename K> void fieldRaw(K k, const char* json)            { key(k); raw(json); }
  template<typename K> void fieldRawP(K k, PGM_P json)                 { key(k); rawP(json); }

  // true if no container was dropped from depth overflow (response is well-formed)
  bool ok() const { return !_bDepthError; }

private:
  void _sep() {
    if (_suppressSep) { _suppressSep = false; return; }
    if (_depth > 0) {
      if (_firstMask & (uint16_t)(1u << _depth)) _firstMask &= (uint16_t)~(1u << _depth);
      else                                        _out.print(',');
    }
  }
  void _openContainer(char brace) {
    if (_depth >= MAX_DEPTH) {                    // overflow: drop the container,
      _bDepthError = true;                        // record it so its matching close
      _suppressSep = false;                       // is skipped too (no dangling key
      _dropped++;                                 // state, no parent over-close).
      return;
    }
    _sep();
    _out.print(brace);
    ++_depth;
    _firstMask |= (uint16_t)(1u << _depth);
  }
  void _closeContainer(char brace) {
    if (_dropped) { _dropped--; return; }  // matching close for an overflow-dropped open
    if (_depth == 0) return;               // unbalanced close: ignore
    --_depth;
    _out.print(brace);
  }

  Print&   _out;
  uint8_t  _depth;
  uint16_t _firstMask;
  bool     _suppressSep;
  bool     _bDepthError;
  uint8_t  _dropped;     // opens dropped at MAX_DEPTH, awaiting their matching close
};

#endif // JSONEMIT_H

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
***************************************************************************
*/
