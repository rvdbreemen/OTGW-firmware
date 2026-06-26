/*
***************************************************************************
**  Program  : jsonChunked.h
**  Version  : v2.0.0-alpha.275
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TRUE chunked / pull-based JSON streaming for the ESP32-S3 REST path
**  (TASK-883). The real fix for the under-flood cbuf "Failed to allocate"
**  resize storm that the alpha.216 heap-tier backpressure gate only MITIGATES.
**
**  WHY: restBeginStream() (webServerCompat.h) writes a whole response into one
**  contiguous AsyncResponseStream cbuf. Under an 8-16 worker flood the internal-
**  RAM heap fragments below the response size, every cbuf grow fails, and
**  AsyncResponseStream::write() logs "Failed to allocate" per write through the
**  slow esp_diagnostics hook until async_tcp misses the 30 s watchdog. The gate
**  caps concurrency; it does not remove the whole-response buffer.
**
**  HOW: AsyncChunkedResponse pulls the body in TCP-window-sized pieces. Its
**  filler is called repeatedly with a monotonically increasing `index` (= bytes
**  produced so far; WebResponses.cpp AsyncChunkedResponse::_fillBuffer passes
**  _filledLength) and must return bytes [index, index+maxLen). We re-run a
**  single-pass JsonEmit closure each call into a WINDOWING Print sink that
**  discards bytes < index and captures [index, index+maxLen). Peak memory is one
**  ~1460 B window plus the small std::function/snapshot the closure owns; there
**  is NO whole-response contiguous buffer, so the resize storm cannot occur even
**  when the heap is fragmented below the response size. Returning 0 (no bytes at
**  or past index) ends the response.
**
**  COST: O(n^2) in response size (each window re-serializes from byte 0 up to its
**  end). n is a few KB and windows are ~1460 B, so ~response/1460 passes; the
**  same re-serialize-per-window trade ADR-145 made (with the now-removed ArduinoJson
**  path), here on JsonEmit. The deliberate "no whole buffer" vs "re-serialize" choice.
**
**  DETERMINISM CONTRACT (critical): the emit closure MUST produce byte-identical
**  output on every call. If a field's TEXT WIDTH changes between window passes
**  (e.g. freeheap 95828 -> 100240, uptime ticking a second, a live temperature),
**  the byte offsets shift and the window misaligns -> corrupt JSON on the wire.
**    - Request-STABLE state (settings.*, config) may be read live: it does not
**      change during the few milliseconds a single response streams.
**    - Any VOLATILE field (heap, uptime, millis-derived, live OT/sensor telemetry,
**      perf counters) MUST be captured in a SNAPSHOT that the closure owns (a
**      heap object captured by value/shared_ptr in the lambda), so every re-run
**      reads the same frozen value. The snapshot is small and fixed-size — far
**      cheaper and non-fragmenting versus the whole-response cbuf it replaces.
**
**  CONCURRENCY: the backpressure gate admits up to REST_MAX_INFLIGHT responses,
**  whose filler callbacks interleave over time as TCP windows ACK. The window
**  sink is stack-local per call (no sharing). Any snapshot MUST be per-response
**  heap (owned by the closure), never file-static, or concurrent chunked
**  responses would clobber each other.
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef JSONCHUNKED_H
#define JSONCHUNKED_H

#include <Arduino.h>
#include <functional>
#include <memory>
#include <ESPAsyncWebServer.h>
#include "jsonEmit.h"

// From webServerCompat.h (single per-request context, safe under async_tcp
// serialization for the binding step; the filler runs later from the async path).
extern AsyncWebServerRequest* currentRequest;
extern bool                   g_responseSent;
void webApplyHeaders(AsyncWebServerResponse* resp);

// Windowing Print sink: receives the FULL response byte stream from a re-run of
// the emit closure, but only copies the bytes that fall in [winStart, winStart+cap)
// into `out`. Bytes before winStart advance the logical position and are dropped;
// bytes at/after winStart fill `out` until it is full (further bytes are counted
// but dropped). written() is how many landed in this window (0 => index is at or
// past the end of the response => the response is complete).
class RestChunkWindow : public Print {
public:
  RestChunkWindow(uint8_t* out, size_t cap, size_t winStart)
    : _out(out), _cap(cap), _start(winStart), _pos(0), _written(0) {}

  size_t write(uint8_t b) override {
    if (_pos >= _start && _written < _cap) _out[_written++] = b;
    _pos++;
    return 1;
  }
  size_t write(const uint8_t* buf, size_t size) override {
    for (size_t i = 0; i < size; ++i) {
      if (_pos >= _start && _written < _cap) _out[_written++] = buf[i];
      ++_pos;
    }
    return size;
  }
  using Print::write;

  size_t written() const { return _written; }

private:
  uint8_t* _out;
  size_t   _cap;
  size_t   _start;
  size_t   _pos;       // logical byte position in the full response
  size_t   _written;   // bytes copied into _out this window
};

// The emit closure: writes the COMPLETE JSON response via the given JsonEmit on
// every call. Must be deterministic (see DETERMINISM CONTRACT above).
using RestEmitFn = std::function<void(JsonEmit&)>;

// Send `emitFn`'s JSON as a true chunked response (no whole-response buffer).
// Sends exactly once; no-op if the request already sent. The closure is moved
// into a shared_ptr so it (and any snapshot it captured) lives across all the
// async filler calls and is freed when the response object is destroyed.
inline void restSendChunked(const char* contentType, RestEmitFn emitFn) {
  if (!currentRequest || g_responseSent) return;
  auto fn = std::make_shared<RestEmitFn>(std::move(emitFn));
  AsyncWebServerResponse* resp = currentRequest->beginChunkedResponse(
      contentType,
      [fn](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
        RestChunkWindow win(buf, maxLen, index);
        JsonEmit je(win);
        (*fn)(je);
        return win.written();   // 0 => nothing at/after index => response complete
      });
  if (!resp) return;            // alloc failure: leave unsent (handler may 503)
  webApplyHeaders(resp);
  currentRequest->send(resp);
  g_responseSent = true;
}

#endif // JSONCHUNKED_H

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED. See the MIT License for details.
*
****************************************************************************
*/
