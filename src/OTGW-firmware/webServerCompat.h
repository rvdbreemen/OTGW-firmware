/*
***************************************************************************
**  Program  : webServerCompat.h
**  Version  : v2.0.0-alpha.324
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  ESPAsyncWebServer bridge for the OTGW web stack (TASK-865.9, ADR-123 Phase 3).
**
**  The HTTP server moved from the synchronous WebServer (one socket per
**  loop() handleClient() poll) onto ESPAsyncWebServer, which runs every
**  request handler on the single AsyncTCP service task ("async_tcp",
**  CONFIG_ASYNC_TCP_RUNNING_CORE=-1, STACK_SIZE=16384, PRIORITY=10,
**  QUEUE_SIZE=64). Handlers are SERIALIZED on that one task: one runs to
**  completion before the next lwIP event is dispatched. That serialization
**  is what makes the file-static request context below safe (no per-request
**  heap state, no concurrency).
**
**  This header is the bridge between the existing imperative-push REST/
**  FSexplorer code (sendStartJsonMap -> N x sendJsonMapEntry -> sendEndJsonMap,
**  serveVersionedAsset, doRedirect) and the pull/callback async server.
**
**  Split by hazard (TASK-865.9 design):
**    - READ side (stateless queries on currentRequest): argCompat/hasArgCompat/
**      headerCompat/... are direct, safe, and keep call sites reading like the
**      old API.
**    - WRITE side (the send-EXACTLY-ONCE hazard): ESPAsyncWebServer faults on a
**      double send and hangs the socket on no send. Every response funnels
**      through this header's helpers, which enforce one finalize per request
**      via g_responseSent. Large/unbounded bodies use chunked callbacks (never
**      buffered whole on the fragmented S3 heap); small bounded JSON uses the
**      retargeted restSend* layer (jsonStuff.ino) writing into g_restStream.
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef WEBSERVERCOMPAT_H
#define WEBSERVERCOMPAT_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

//=====[ Async HTTP server (port 80) ]=========================================
// Single point of instantiation in networkStuff.ino (ADR-044). seq10 (WS) and
// seq11 (OTA) attach to THIS instance.
extern AsyncWebServer server;

//=====[ Per-request context (file-static, safe under async_tcp serialization)]=
// currentRequest is set at the top of every route handler and read by the
// compat accessors below. g_restStream is the lazily-allocated response stream
// for the small-JSON path; g_responseSent guards the send-once invariant.
extern AsyncWebServerRequest* currentRequest;
extern AsyncResponseStream*   g_restStream;
extern bool                   g_responseSent;

// Pending response headers. The sync WebServer let callers stage headers with
// sendHeader() before send(); the async API attaches headers to the response
// object instead. webPushHeader() queues a header; the webSend*/restFinalize
// emitters drain the queue onto the response right before send().
#ifndef WEB_MAX_PENDING_HEADERS
#define WEB_MAX_PENDING_HEADERS 6
#endif
#ifndef WEB_HEADER_NAME_LEN
#define WEB_HEADER_NAME_LEN 32
#endif
#ifndef WEB_HEADER_VALUE_LEN
#define WEB_HEADER_VALUE_LEN 160
#endif

struct WebPendingHeaders {
  char   name[WEB_MAX_PENDING_HEADERS][WEB_HEADER_NAME_LEN];
  char   value[WEB_MAX_PENDING_HEADERS][WEB_HEADER_VALUE_LEN];
  uint8_t count;
};
extern WebPendingHeaders g_pendingHeaders;

// Captured raw POST/PUT body. The sync WebServer exposed the request body as
// arg("plain") / arg(0); the async server delivers it through the global
// onRequestBody() hook (registered in networkStuff.ino) which fills this buffer
// before the matching route handler runs. Safe as a single file-static under
// async_tcp serialization: a request's body is fully received before its handler
// executes, and handlers do not overlap.
// 2560 covers the largest body the REST API consumes: updateAllDallasLabels
// caps the payload at 2048 bytes, so the capture buffer must be at least that
// plus headroom. Most bodies are tiny ("21.5", {"value":1500}); this is sized
// for the worst case, not the common one.
#ifndef WEB_BODY_MAX_LEN
#define WEB_BODY_MAX_LEN 2560
#endif
struct WebRequestBody {
  char     data[WEB_BODY_MAX_LEN];
  size_t   len;
  void*    owner;   // request pointer the body belongs to (sanity match)
};
extern WebRequestBody g_requestBody;

// Append a body chunk (called from the onRequestBody hook). index==0 resets.
inline void webCaptureBody(void* req, const uint8_t* data, size_t len, size_t index, size_t total) {
  (void)total;
  if (index == 0) { g_requestBody.len = 0; g_requestBody.owner = req; }
  size_t room = (sizeof(g_requestBody.data) - 1) - g_requestBody.len;
  size_t take = (len < room) ? len : room;
  memcpy(g_requestBody.data + g_requestBody.len, data, take);
  g_requestBody.len += take;
  g_requestBody.data[g_requestBody.len] = '\0';
}

//=====[ Request-lifecycle helpers ]===========================================

// Bind the per-request context at the top of a route handler and clear all
// send-once / header state. ALWAYS call this first in a handler.
inline void webBeginRequest(AsyncWebServerRequest* req) {
  currentRequest          = req;
  g_restStream            = nullptr;
  g_responseSent          = false;
  g_pendingHeaders.count  = 0;
}

// Queue a header to be attached to the next response. Silently drops past the
// fixed slot count (the firmware never stages more than WEB_MAX_PENDING_HEADERS
// on a single response). PROGMEM-safe overloads cover the F() call sites.
inline void webPushHeader(const char* name, const char* value) {
  if (g_pendingHeaders.count >= WEB_MAX_PENDING_HEADERS) return;
  uint8_t i = g_pendingHeaders.count++;
  strlcpy(g_pendingHeaders.name[i], name, WEB_HEADER_NAME_LEN);
  strlcpy(g_pendingHeaders.value[i], value, WEB_HEADER_VALUE_LEN);
}
inline void webPushHeader(const __FlashStringHelper* name, const __FlashStringHelper* value) {
  if (g_pendingHeaders.count >= WEB_MAX_PENDING_HEADERS) return;
  uint8_t i = g_pendingHeaders.count++;
  strlcpy_P(g_pendingHeaders.name[i], (PGM_P)name, WEB_HEADER_NAME_LEN);
  strlcpy_P(g_pendingHeaders.value[i], (PGM_P)value, WEB_HEADER_VALUE_LEN);
}
inline void webPushHeader(const __FlashStringHelper* name, const char* value) {
  if (g_pendingHeaders.count >= WEB_MAX_PENDING_HEADERS) return;
  uint8_t i = g_pendingHeaders.count++;
  strlcpy_P(g_pendingHeaders.name[i], (PGM_P)name, WEB_HEADER_NAME_LEN);
  strlcpy(g_pendingHeaders.value[i], value, WEB_HEADER_VALUE_LEN);
}

// Drain the pending-header queue onto a response object, then clear it.
inline void webApplyHeaders(AsyncWebServerResponse* resp) {
  for (uint8_t i = 0; i < g_pendingHeaders.count; i++) {
    resp->addHeader(g_pendingHeaders.name[i], g_pendingHeaders.value[i]);
  }
  g_pendingHeaders.count = 0;
}

//=====[ READ side — stateless queries on currentRequest ]=====================
//
// CAUTION: argCompat()/headerCompat()/hostHeaderCompat()/uriCompat() each return
// a pointer into a SINGLE shared static buffer per function. Do NOT use two calls
// to the same accessor in one expression, e.g. f(argCompat("a"), argCompat("b")) —
// the second call clobbers the first's buffer before the call. Copy out (strlcpy)
// before the next call. All current call sites obey this; new editors must too.

// argCompat preserves the SYNC WebServer arg() MERGED semantics: the sync server
// returned a value whether it came from the query string OR the POST body. The
// async server SEPARATES them (getParam(name, isPost)), so we check POST first
// then GET. Returns "" when absent. The returned pointer is valid only until the
// next argCompat() call (single static buffer; safe under serialization).
inline const char* argCompat(const char* name) {
  static char buf[160];
  buf[0] = '\0';
  if (!currentRequest) return buf;
  const AsyncWebParameter* p = currentRequest->getParam(name, true);   // POST body
  if (!p) p = currentRequest->getParam(name, false);                   // query string
  if (p) strlcpy(buf, p->value().c_str(), sizeof(buf));
  return buf;
}
inline bool hasArgCompat(const char* name) {
  if (!currentRequest) return false;
  return currentRequest->hasParam(name, true) || currentRequest->hasParam(name, false);
}

// Raw request body (the sync WebServer's arg("plain") / arg(0)). Returns the
// buffer captured by the onRequestBody hook for the current request, or "" if
// none. Only valid for the in-flight request (owner sanity check).
inline const char* bodyCompat() {
  if (currentRequest && g_requestBody.owner == currentRequest && g_requestBody.len > 0) {
    return g_requestBody.data;
  }
  return "";
}
inline bool hasBodyCompat() {
  return currentRequest && g_requestBody.owner == currentRequest && g_requestBody.len > 0;
}

// F()-keyed overloads. The sync WebServer's "plain" pseudo-arg is the raw POST
// body; map it to the captured body. Any other F() key falls through to the
// normal POST-then-GET param lookup.
inline const char* argCompat(const __FlashStringHelper* name) {
  char nbuf[24];
  strlcpy_P(nbuf, (PGM_P)name, sizeof(nbuf));
  if (strcmp_P(nbuf, PSTR("plain")) == 0) return bodyCompat();
  return argCompat(nbuf);
}
inline bool hasArgCompat(const __FlashStringHelper* name) {
  char nbuf[24];
  strlcpy_P(nbuf, (PGM_P)name, sizeof(nbuf));
  if (strcmp_P(nbuf, PSTR("plain")) == 0) return hasBodyCompat();
  return hasArgCompat(nbuf);
}

// Header value (request side). Returns "" when absent. Pointer valid until next call.
inline const char* headerCompat(const char* name) {
  static char buf[160];
  buf[0] = '\0';
  if (!currentRequest) return buf;
  const AsyncWebHeader* h = currentRequest->getHeader(name);
  if (h) strlcpy(buf, h->value().c_str(), sizeof(buf));
  return buf;
}
inline const char* headerCompat(const __FlashStringHelper* name) {
  char nbuf[40];
  strlcpy_P(nbuf, (PGM_P)name, sizeof(nbuf));
  return headerCompat(nbuf);
}
inline bool hasHeaderCompat(const __FlashStringHelper* name) {
  char nbuf[40];
  strlcpy_P(nbuf, (PGM_P)name, sizeof(nbuf));
  if (!currentRequest) return false;
  return currentRequest->hasHeader(nbuf);
}

// Host header (used by the CSRF same-origin check).
inline const char* hostHeaderCompat() {
  static char buf[80];
  buf[0] = '\0';
  if (currentRequest) strlcpy(buf, currentRequest->host().c_str(), sizeof(buf));
  return buf;
}

// Map the ESPAsyncWebServer request method to the firmware's HTTPMethod. These
// are TWO DIFFERENT enums that share member names but NOT values: ESPAsyncWebServer
// AsyncWebRequestMethod is a namespaced bitmask (GET=1<<1=2, POST=1<<3=8, ...),
// while HTTPMethod is http_parser's sequential enum http_method (GET=1, POST=3, ...).
// A raw `(HTTPMethod)request->method()` cast therefore mislabels every request: a
// GET (async 2) became http_parser HTTP_HEAD(2), so every GET-guarded /api/v2
// endpoint returned 405 and POST/PUT auth checks misfired (405/403 storm, TASK-869).
// Map by NAME via a switch so it stays correct if either enum is renumbered.
inline HTTPMethod methodCompat() {
  if (!currentRequest) return HTTP_GET;
  switch (currentRequest->method()) {
    case AsyncWebRequestMethod::HTTP_GET:     return HTTP_GET;
    case AsyncWebRequestMethod::HTTP_POST:    return HTTP_POST;
    case AsyncWebRequestMethod::HTTP_PUT:     return HTTP_PUT;
    case AsyncWebRequestMethod::HTTP_DELETE:  return HTTP_DELETE;
    case AsyncWebRequestMethod::HTTP_PATCH:   return HTTP_PATCH;
    case AsyncWebRequestMethod::HTTP_HEAD:    return HTTP_HEAD;
    case AsyncWebRequestMethod::HTTP_OPTIONS: return HTTP_OPTIONS;
    default:                                  return HTTP_ANY;
  }
}
inline const char* uriCompat() {
  static char buf[80];
  buf[0] = '\0';
  if (currentRequest) strlcpy(buf, currentRequest->url().c_str(), sizeof(buf));
  return buf;
}

// HTTP Basic Auth check against the current request.
inline bool authenticateCompat(const char* user, const char* pass) {
  return currentRequest ? currentRequest->authenticate(user, pass) : false;
}

//=====[ WRITE side — send EXACTLY ONCE ]======================================

// Trivial one-shot send (bounded body). Drains pending headers, sends once.
inline void webSend(int code, const __FlashStringHelper* contentType, const char* body) {
  if (!currentRequest || g_responseSent) return;
  AsyncWebServerResponse* resp = currentRequest->beginResponse(code, String(contentType), body);
  webApplyHeaders(resp);
  currentRequest->send(resp);
  g_responseSent = true;
}
inline void webSend(int code, const __FlashStringHelper* contentType, const __FlashStringHelper* body) {
  if (!currentRequest || g_responseSent) return;
  AsyncWebServerResponse* resp = currentRequest->beginResponse(code, String(contentType), String(body));
  webApplyHeaders(resp);
  currentRequest->send(resp);
  g_responseSent = true;
}
// Status-only send (e.g. 204, 304, 414).
inline void webSendStatus(int code) {
  if (!currentRequest || g_responseSent) return;
  AsyncWebServerResponse* resp = currentRequest->beginResponse(code);
  webApplyHeaders(resp);
  currentRequest->send(resp);
  g_responseSent = true;
}
// PROGMEM body send.
inline void webSendP(int code, PGM_P contentType, PGM_P body) {
  if (!currentRequest || g_responseSent) return;
  AsyncWebServerResponse* resp =
      currentRequest->beginResponse(code, String(FPSTR(contentType)), String(FPSTR(body)));
  webApplyHeaders(resp);
  currentRequest->send(resp);
  g_responseSent = true;
}

// ADR-147 D4.1: static-file backpressure gate. Defined in restAPI.ino (next to the
// sibling REST gate, where the platform heap shims are in scope); forward-declared here
// so webSendFile can admit/reject before touching LittleFS. async_tcp-task-local.
bool webFileGateTryAdmit();   // true => admitted (counter incremented); false => caller 503s
void webFileGateRelease();    // decrement; call from request->onDisconnect

// Stream a file from LittleFS (or the .gz sibling — Content-Encoding handled by
// the response). Drains pending headers, sends once.
inline void webSendFile(const char* path, const char* contentType, bool gzip) {
  if (!currentRequest || g_responseSent) return;
  // ADR-147 D4.1: refuse a new file serve under heap pressure / too much concurrency,
  // BEFORE any LittleFS work, so an esp_littlefs FD-struct alloc cannot fail into a hung
  // connection that drains the lwIP PCB pool (TASK-879). A normal single-browser asset
  // burst (<=6 parallel) is never throttled while the heap is healthy; under a flood the
  // heap-tier clamp tightens the cap toward 1 and the excess gets a cheap 503.
  if (!webFileGateTryAdmit()) {
    // TASK-960: tell the client this is transient backpressure, not a hard error,
    // so a cooperative loader retries promptly (the gate frees a slot within ~1s
    // as in-flight serves finish). The cap itself stays allocation-bound (cap 1
    // when maxBlock<16000): two concurrent LittleFS serves cannot be guaranteed to
    // allocate at that point (the TASK-879 hung-PCB hazard), so the floor is held
    // and the client-side retry — not a higher cap — is what makes a multi-asset
    // page load reliably under heap pressure.
    webPushHeader(F("Retry-After"), F("1"));
    webSendStatus(503);
    return;
  }
  currentRequest->onDisconnect([]() { webFileGateRelease(); });
  // Missing-file guard (ADR-139): beginResponse(LittleFS, <missing>) yields an
  // invalid-source response that send() turns into a 500 (or a hung connection on
  // some ESPAsyncWebServer forks), never a clean 404. Check first.
  if (!LittleFS.exists(path)) {
    AsyncWebServerResponse* nf = currentRequest->beginResponse(404, String(F("text/plain")), String(F("File not found")));
    webApplyHeaders(nf);
    currentRequest->send(nf);
    g_responseSent = true;
    return;
  }
  AsyncWebServerResponse* resp = currentRequest->beginResponse(LittleFS, path, contentType);
  if (gzip) resp->addHeader(F("Content-Encoding"), F("gzip"));
  webApplyHeaders(resp);
  currentRequest->send(resp);
  g_responseSent = true;
}
inline void webSendFile(const char* path, const __FlashStringHelper* contentType, bool gzip) {
  char ctbuf[48];
  strlcpy_P(ctbuf, (PGM_P)contentType, sizeof(ctbuf));
  webSendFile(path, ctbuf, gzip);
}

// Request HTTP Basic Auth (sends a 401 challenge). Counts as the one send.
inline void webRequestAuth() {
  if (!currentRequest || g_responseSent) return;
  currentRequest->requestAuthentication();
  g_responseSent = true;
}

//=====[ Small-JSON path — retargeted restSend* layer (jsonStuff.ino) ]=========
// restBeginStream() lazily opens the AsyncResponseStream the restSend* helpers
// write into; restFinalize() sends it exactly once. Both are no-ops if already
// done, so an early-return handler that already sent (auth fail etc.) is safe.
inline AsyncResponseStream* restBeginStream(const char* contentType,
                                           size_t bufferSize = RESPONSE_STREAM_BUFFER_SIZE) {
  if (!currentRequest || g_responseSent) return nullptr;
  if (!g_restStream) {
    g_restStream = currentRequest->beginResponseStream(contentType, bufferSize);
    webApplyHeaders(g_restStream);
  }
  return g_restStream;
}
inline void restFinalize() {
  if (!currentRequest || g_responseSent) return;
  if (g_restStream) {
    currentRequest->send(g_restStream);
    g_restStream  = nullptr;
    g_responseSent = true;
  }
}

// ADR-146 / TASK-886: the ArduinoJson chunked-pull path (restSendJson(JsonDocument&),
// RestJsonStream, JsonChunkWindow) was removed with the full ArduinoJson revert.
// REST handlers now emit directly via restBeginStream() + JsonEmit + restFinalize()
// (single-pass into the AsyncResponseStream; no JsonDocument, no per-chunk re-serialize).

#endif // WEBSERVERCOMPAT_H

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
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
****************************************************************************
*/
