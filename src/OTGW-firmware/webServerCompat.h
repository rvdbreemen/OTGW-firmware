/*
***************************************************************************
**  Program  : webServerCompat.h
**  Version  : v2.0.0-beta.196
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
**  FSexplorer code (sendJsonMapEntry and the restSend* writers,
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
**  TERMS OF USE: GNU GPLv3. See bottom of file.
***************************************************************************
*/

#ifndef WEBSERVERCOMPAT_H
#define WEBSERVERCOMPAT_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <platform.h>   // platformMaxFreeBlock() for restBeginStreamSized (TASK-1736)
#include <new>
#include <utility>
#include "version.h"   // _VERSION_GITHASH for webSendGzipPage's ETag

//=====[ Async HTTP server (port 80) ]=========================================
// Single point of instantiation in networkStuff.ino (ADR-044). seq10 (WS) and
// seq11 (OTA) attach to THIS instance.
#define WEB_HTTP_PORT 80
extern AsyncWebServer server;

//=====[ Per-request context (file-static, safe under async_tcp serialization)]=
// currentRequest is set at the top of every route handler and read by the
// compat accessors below. g_restStream is the lazily-allocated response stream
// for the small-JSON path; g_responseSent guards the send-once invariant.
// g_restSlotHeld records the current request's REST slot for webArmSlotRelease().
// Static-file slots belong to their response, not the disconnect callback.
extern AsyncWebServerRequest* currentRequest;
extern AsyncResponseStream*   g_restStream;
extern uint8_t                g_restSizeClass;   // TASK-1736: RestSizeClass of g_restStream
extern uint16_t               g_restSizeCap;     // TASK-1736: initial buffer g_restStream asked for
extern bool                   g_responseSent;
extern bool                   g_restSlotHeld;

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
//
// TASK-1565: the capture is released when its request is destroyed. The async
// server delivers a form-urlencoded body (curl -d default) as POST params and
// never calls this hook for it, so the previous request's body stayed in the
// buffer with owner==<freed address>, and the next request allocated at that same
// address passed the owner match and read the old body. The server runs the
// request's onDisconnect handler in AsyncWebServerRequest::_onDisconnect() just
// before it deletes the request (one request per connection), so releasing there
// closes the hole for every request type. The server keeps ONE disconnect
// callback per request, so the body release and the gate-slot releases are armed
// together by webArmDisconnect() (below), never separately. Not covered: a
// request destroyed while its client is still connected (the destructor clears
// the callback instead).
inline void webBodyReleaseFor(const void* req) {
  if (g_requestBody.owner == req) { g_requestBody.owner = nullptr; g_requestBody.len = 0; g_requestBody.data[0] = '\0'; }
}
void restSlotRelease();       // REST gate (restAPI.ino)
// The one disconnect callback releases the REST slot and captured request body.
inline void webArmDisconnect(AsyncWebServerRequest* req, bool restHeld) {
  req->onDisconnect([req, restHeld]() {
    if (restHeld) restSlotRelease();
    webBodyReleaseFor(req);
  });
}
inline void webCaptureBody(AsyncWebServerRequest* req, const uint8_t* data, size_t len, size_t index, size_t total) {
  (void)total;
  if (index == 0) {
    g_requestBody.len = 0;
    g_requestBody.owner = req;
    g_requestBody.data[0] = '\0';
    // The body normally arrives before the route handler takes a gate. If this request
    // already holds one, keep its release in the same callback.
    const bool mine = (currentRequest == req);
    webArmDisconnect(req, mine && g_restSlotHeld);
  }
  size_t room = (sizeof(g_requestBody.data) - 1) - g_requestBody.len;
  size_t take = (len < room) ? len : room;
  memcpy(g_requestBody.data + g_requestBody.len, data, take);
  g_requestBody.len += take;
  g_requestBody.data[g_requestBody.len] = '\0';
}

//=====[ Request-lifecycle helpers ]===========================================

// Bind the per-request context at the top of a route handler and clear all
// send-once / header state. ALWAYS call this first in a handler.
#ifndef WEB_TCP_SEND_CAP
#define WEB_TCP_SEND_CAP 2872
#endif
inline void webBeginRequest(AsyncWebServerRequest* req) {
  // ADR-189 (TASK-1162): when a response gets no ACK for the ack timeout (5 s), abort the
  // connection instead of the library's graceful close. tcp_close() queues the FIN behind
  // the unsent body, which cannot drain under memory pressure, so the client sees a body
  // that stops and waits for its own timeout. tcp_abort() frees the queue and sends a RST
  // the client sees at once.
  if (req && req->client()) {
    // TASK-1673: each web connection queues at most 2 x MSS (2872 B) in lwIP instead of the
    // full TCP_SND_BUF (5744 B); two streams overlapping held 11.5 KB at the two-tab heap dip.
    platformTcpCapSendBuffer(req->client()->pcb(), WEB_TCP_SEND_CAP);
    req->client()->onTimeout([](void*, AsyncClient* c, uint32_t) {
      state.heapdiag.iWebAckAbortCount++;
      c->abort();
    }, nullptr);
  }
  currentRequest          = req;
  g_restStream            = nullptr;
  g_restSizeClass         = 0;      // TASK-1736: REST_SIZE_NONE (enum is declared further down)
  g_restSizeCap           = 0;
  g_responseSent          = false;
  g_restSlotHeld          = false;
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
// none. Only valid for the in-flight request.
//
// The owner match alone is not enough: g_requestBody is never cleared, and a
// body-less request allocated at the same heap address as the previous one
// passes it and reads the stale body (TASK-1158: POST /sat/enable/1 without a
// body parsed a preceding settings POST's "false"). The request's own
// Content-Length decides whether it has a body at all.
inline bool hasBodyCompat() {
  return currentRequest && currentRequest->contentLength() > 0 &&
         g_requestBody.owner == currentRequest && g_requestBody.len > 0;
}
inline const char* bodyCompat() {
  return hasBodyCompat() ? g_requestBody.data : "";
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
// PROGMEM body send without a heap copy. webSendP() builds a String of the body and the
// basic response keeps a second one, so a 47 KB page needs about 94 KB of contiguous heap.
// This overload streams the body straight from flash (AsyncProgmemResponse). For large
// static pages such as the /update page.
inline void webSendPStream(int code, PGM_P contentType, PGM_P body) {
  if (!currentRequest || g_responseSent) return;
  AsyncWebServerResponse* resp =
      currentRequest->beginResponse(code, String(FPSTR(contentType)),
                                    reinterpret_cast<const uint8_t*>(body), strlen_P(body));
  webApplyHeaders(resp);
  currentRequest->send(resp);
  g_responseSent = true;
}

// TASK-1640 (ADR-215): a gzipped page from app flash (scripts/webui_pages.py), streamed
// without a heap copy and sent with Content-Encoding: gzip. With etagPath, the ETag is the
// firmware hash plus that path (the page changes only with the firmware) and a matching
// If-None-Match gets 304 with no body.
// TASK-1696: the same for any binary body with an explicit length (no strlen, so zero bytes are
// fine), optionally gzipped. Takes neither the REST gate nor a web file-gate slot.
inline void webSendFlashBody(PGM_P contentType, const uint8_t* body, size_t len, const char* etagPath, bool gzip) {
  if (!currentRequest || g_responseSent) return;
  webPushHeader(F("Cache-Control"), F("no-cache"));
  if (etagPath) {
    char etag[64];
    snprintf_P(etag, sizeof(etag), PSTR("\"%s-%s\""), _VERSION_GITHASH, etagPath);
    webPushHeader(F("ETag"), etag);
    if (hasHeaderCompat(F("If-None-Match")) && strcmp(headerCompat(F("If-None-Match")), etag) == 0) {
      webSendStatus(304);
      return;
    }
  }
  if (gzip) webPushHeader(F("Content-Encoding"), F("gzip"));
  AsyncWebServerResponse* resp = currentRequest->beginResponse(200, String(FPSTR(contentType)), body, len);
  webApplyHeaders(resp);
  currentRequest->send(resp);
  g_responseSent = true;
}
inline void webSendGzipPage(PGM_P contentType, const uint8_t* gz, size_t len, const char* etagPath = nullptr) {
  webSendFlashBody(contentType, gz, len, etagPath, true);
}

// Active-file admission is silent; only actual safety/queue/deadline refusals
// count as 503s. These helpers live beside the unchanged REST gate.
bool webFileGateTryAdmit();
void webAckLowObserve(uint32_t windowMin, uint32_t freeBefore);
void webFileGateRelease();
bool webFileGateCanSend(bool reserve);
void webFileGateRejected();

// ESPAsyncWebServer keeps ONE disconnect callback per request. Keep body cleanup
// and REST release together; the response independently owns its file slot.
inline void webArmSlotRelease() {
  if (!currentRequest) return;
  if (g_restSlotHeld) {
    webArmDisconnect(currentRequest, g_restSlotHeld);
  }
}

inline AsyncWebServerResponse* webFileErrorResponse(AsyncWebServerRequest* request, int code) {
  AsyncWebServerResponse* response;
  if (code == 404) {
    response = request->beginResponse(404, String(F("text/plain")), String(F("File not found")));
  } else if (code == 500) {
    response = request->beginResponse(500, String(F("text/plain")), String(F("Invalid data in handler")));
  } else {
    response = request->beginResponse(code);
  }
  if (response) {
    response->addHeader(F("Cache-Control"), F("no-store"));
    if (code == 503) response->addHeader(F("Retry-After"), F("1"));
  }
  return response;
}

// A waiting response owns only bounded metadata, not an open file or send buffer.
// ESPAsyncWebServer polls _ack on async_tcp and deletes this with its request.
class WebQueuedFileResponse : public AsyncWebServerResponse {
  struct Queue {
    WebQueuedFileResponse* entries[4];
    uint8_t count;
    bool pumping;
  };
  static Queue& queue() { static Queue pending = {}; return pending; }
  char _path[128];
  char _mime[48];
  uint32_t _queuedAt;
  bool _held;
  bool _gzip;
  bool _deferred = false;
  AsyncWebServerRequest* _request = nullptr; // Owned by the same live request; async_tcp only.
  AsyncWebServerResponse* _inner = nullptr;

  static void observeDepth() {
    state.heapdiag.iWebfilePending = queue().count;
    if (queue().count > state.heapdiag.iWebfilePendingHwm)
      state.heapdiag.iWebfilePendingHwm = queue().count;
  }
  void observeWait() {
    const uint32_t elapsed = uint32_t(millis() - _queuedAt);
    if (elapsed > state.heapdiag.iWebfileMaxWaitMs) state.heapdiag.iWebfileMaxWaitMs = elapsed;
  }
  void dequeue() {
    Queue& pending = queue();
    for (uint8_t i = 0; i < pending.count; ++i) {
      if (pending.entries[i] != this) continue;
      for (uint8_t j = i + 1; j < pending.count; ++j) pending.entries[j - 1] = pending.entries[j];
      pending.entries[--pending.count] = nullptr;
      observeDepth();
      break;
    }
  }
  void release() {
    if (_held) { webFileGateRelease(); _held = false; }
  }
  bool error(AsyncWebServerRequest* request, int code) {
    dequeue();
    delete _inner;
    _inner = nullptr;
    release();
    _headers.clear();  // A refusal must not inherit a successful asset's validators.
    _inner = webFileErrorResponse(request, code);
    if (_inner) return true;
    _state = RESPONSE_FAILED;
    request->abort();
    if (request->client()->pcb()) request->client()->abort(); // request.abort() is a no-op after _sent.
    return false;
  }
  void start(AsyncWebServerRequest* request) {
    if (!_held) {
      if (uint32_t(millis() - _queuedAt) >= 10000U) {
        observeWait();
        state.heapdiag.iWebfileTimeout++;
        _deferred = false;
        webFileGateRejected();
        if (!error(request, 503)) return;
      } else if (queue().entries[0] != this || !webFileGateCanSend(true) || !webFileGateTryAdmit()) {
        return;
      } else {
        observeWait();
        if (_deferred) { state.heapdiag.iWebfileAdmitted++; _deferred = false; }
        dequeue();
        _held = true;
      }
    }
    if (!_inner) {
      // Check only after admission: even exists() can allocate filesystem state.
      if (!LittleFS.exists(_path)) {
        if (!error(request, 404)) return;
      } else {
        // TASK-1673: open the file with a 512-byte stdio buffer; the default is the 4 KB
        // littlefs block size, held per stream until the response ends.
        File content = LittleFS.open(_path, "r");
        if (content) content.setBufferSize(512);
        _inner = content ? request->beginResponse(content, String(_path), (const char*)_mime) : nullptr;
        if (!_inner || !_inner->_sourceValid()) {
          if (!error(request, 500)) return;
        } else if (_gzip) {
          _inner->addHeader(F("Content-Encoding"), F("gzip"));
        }
      }
    }
    for (auto& header : _headers) _inner->addHeader(std::move(header));
    _headers.clear();
    _code = _inner->code();
    request->client()->setRxTimeout(0);
    _inner->_respond(request);
  }

public:
  static bool hasPending() { return queue().count != 0; }
  static bool queueFull() { return queue().count == 4; }
  static uint16_t txHolding() {
    PlatformTcpIdleConnection idle[4];
    uint8_t count = 0;
    for (uint8_t i = 0; i < queue().count; ++i) {
      AsyncWebServerRequest* request = queue().entries[i]->_request;
      if (!request || !request->client()) continue;
      idle[count++] = {request->client()->pcb(), request->client()};
    }
    return platformTcpTxHoldingOnPort(WEB_HTTP_PORT, idle, count);
  }
  static void pump() {
    Queue& pending = queue();
    if (pending.pumping) return;
    pending.pumping = true;
    for (uint8_t i = 0; i < 4 && pending.count; ++i) {
      WebQueuedFileResponse* head = pending.entries[0];
      if (!head->_request || !head->_request->client()->canSend()) break;
      head->start(head->_request);
      if (pending.count && pending.entries[0] == head) break;
    }
    pending.pumping = false;
  }
  WebQueuedFileResponse(const char* path, const char* mime, bool gzip, bool admitted)
      : _queuedAt(millis()), _held(admitted), _gzip(gzip) {
    strlcpy(_path, path, sizeof(_path));
    strlcpy(_mime, mime, sizeof(_mime));
    if (!admitted) { queue().entries[queue().count++] = this; observeDepth(); }
  }
  ~WebQueuedFileResponse() override {
    if (_deferred && !_held && !_inner) {
      observeWait();
      state.heapdiag.iWebfileCancelled++;
    }
    dequeue();
    delete _inner;  // Close the file before admitting another stream.
    release();
    pump();
  }
  bool _sourceValid() const override { return true; }
  bool _started() const override { return _inner && _inner->_started(); }
  bool _finished() const override { return _state == RESPONSE_FAILED || (_inner && _inner->_finished()); }
  bool _failed() const override { return _state == RESPONSE_FAILED || (_inner && _inner->_failed()); }
  void _respond(AsyncWebServerRequest* request) override {
    // Native RX expiry precedes response polling: leave 1s to emit the 10s 503.
    // shortcut: zero-window receive trickles can extend waits; use transport-level expiry if absolute socket lifetime must be bounded.
    _request = request;
    if (!_held) request->client()->setRxTimeout(11);
    if (_held) start(request);
    pump();
    if (_state != RESPONSE_FAILED && !_held && !_inner && !_deferred) {
      _deferred = true;
      state.heapdiag.iWebfileQueued++;
    }
  }
  size_t _ack(AsyncWebServerRequest* request, size_t len, uint32_t time) override {
    if (_state == RESPONSE_FAILED) return 0;
    if (_inner) {
      // TASK-1679: did the heap window minimum drop inside the library's ack (send-buffer
      // copy, tcp_write pbufs)? Only a drop within one window generation counts.
      const uint32_t gen = platformHeapWinGen();
      const uint32_t before = platformHeapWindowMin();
      const uint32_t freeBefore = platformFreeHeap();
      const size_t sent = _inner->_ack(request, len, time);
      const uint32_t after = platformHeapWindowMin();
      if (before && after && after < before && gen == platformHeapWinGen()) webAckLowObserve(after, freeBefore);
      return sent;
    }
    observeWait();
    if (uint32_t(millis() - _queuedAt) >= 10000U) start(request);
    pump();
    return 0;
  }
};

inline void webSendFileError(int code) {
  if (!currentRequest || g_responseSent) return;
  g_pendingHeaders.count = 0;
  AsyncWebServerResponse* response = webFileErrorResponse(currentRequest, code);
  g_responseSent = true;
  // Abort the request too, or the library tries to synthesize a 501 afterward.
  if (response) currentRequest->send(response);
  else currentRequest->abort();
}

// Stream with the standard file response after FIFO admission, preserving staged
// headers and library content-type/gzip behavior. No main-loop HTTP ownership.
inline void webSendFile(const char* path, const char* contentType, bool gzip) {
  if (!currentRequest || g_responseSent) return;
  // Deferred metadata must never truncate into a different path or MIME type.
  if (!path || strnlen(path, 128) >= 128) { webSendFileError(414); return; }
  if (!contentType || strnlen(contentType, 48) >= 48) { webSendFileError(400); return; }
  WebQueuedFileResponse::pump();
  const bool admitted = !WebQueuedFileResponse::hasPending() &&
                        webFileGateCanSend(false) && webFileGateTryAdmit();
  if (!admitted && WebQueuedFileResponse::queueFull()) {
    webFileGateRejected();
    webSendFileError(503);
    return;
  }
  // File slot is response-owned, never also armed in the REST/body callback.
  AsyncWebServerResponse* response = new (std::nothrow) WebQueuedFileResponse(path, contentType, gzip, admitted);
  if (!response) {
    if (admitted) webFileGateRelease();
    g_pendingHeaders.count = 0;
    g_responseSent = true;
    currentRequest->abort();
    return;
  }
  webApplyHeaders(response);
  currentRequest->send(response);
  g_responseSent = true;
}
inline void webSendFile(const char* path, const __FlashStringHelper* contentType, bool gzip) {
  if (!contentType || strlen_P((PGM_P)contentType) >= 48) { webSendFileError(400); return; }
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
//=====[ TASK-1736: presized response streams for the polled endpoints ]========
// AsyncResponseStream starts with a 1460 B cbuf and, on every write that does not
// fit, calls cbuf::resizeAdd(exact shortfall) (ESPAsyncWebServer 3.11.2
// WebResponses.cpp:926-941). cbuf::resize (core 3.3.5 cbuf.cpp:75-133) does not
// realloc: it creates a new ring buffer (calloc header + malloc storage), mallocs a
// temp copy of the content, then frees the temp and the old buffer. JsonEmit prints
// 1-20 B at a time, so past 1460 B nearly every print is a full recreate: a staircase
// of slightly larger blocks. On a board without PSRAM, otmonitor polled every 2 s
// per tab leaves free heap split into holes just too small for the next step.
// The large bodies (settings, sat/status, device/info, debug) already moved to
// restSendChunked() for this reason (TASK-883); the polled stream endpoints did not.
//
// Fix without touching the library: the polled stream endpoints whose body exceeds
// 1460 B ask for a fixed, endpoint-specific buffer upfront. Every poll then makes one
// allocation of the same size, so the allocator can reuse the hole the previous
// response left. A body longer than the presize still works: the library grows the
// tail as before. If the heap is too tight for the presized block plus headroom, keep
// today's 1460 B start: a failed upfront cbuf leaves _buf NULL with size 0
// (cbuf.cpp:53-54), and the staircase would then start from 1-20 B instead.
//
// The sizes are ESTIMATES from the emit code (field counts, loop bounds), not
// measurements. restFinalize logs the real body length once per endpoint per boot and
// on every overflow, so the bench measures them; adjust kRestPresize from that log.
//
// The pure sizing rule and the table sit between sentinels so test/host can slice them.
// ---- host-testable stream size hint: BEGIN ----
static const size_t REST_PRESIZE_HEADROOM = 4096;   // maxblk must exceed the presize by this much

enum RestSizeClass : uint8_t {
  REST_SIZE_NONE = 0,     // unclassed: default 1460 B start
  REST_SIZE_OTMONITOR,    // v2/otgw/otmonitor: 38 entries + Dallas sensors, ~3 KB; polled every 2 s per tab
  REST_SIZE_BLE_ROSTER,   // v2/sat/ble: 0-8 sensors x 12 fields, ~0.3-2.4 KB; every 4 s while the card is open
  REST_SIZE_WEATHER,      // v2/sat/weather: ~20 fields + 4 x 24 forecast values, ~1-1.3 KB; every 60 s
  REST_SIZE_COUNT
};
static const uint16_t kRestPresize[REST_SIZE_COUNT] = { 0, 4096, 3072, 2048 };

// Initial stream buffer: the endpoint's presize when it is larger than the default
// and the heap has room for it plus headroom; otherwise the default.
inline size_t restChooseStreamSize(size_t want, size_t maxBlk, size_t dflt) {
  if (want <= dflt) return dflt;
  if (maxBlk < want + REST_PRESIZE_HEADROOM) return dflt;
  return want;
}
// ---- host-testable stream size hint: END ----

// Defined in restAPI.ino: logs the body length once per class per boot and on overflow.
void restStreamSizeReport(uint8_t cls, size_t cap, size_t len);

// Presized variant of restBeginStream() for a polled endpoint.
inline AsyncResponseStream* restBeginStreamSized(const char* contentType, RestSizeClass cls) {
  if (!currentRequest || g_responseSent) return nullptr;
  if (!g_restStream) {
    const uint8_t idx = (cls < REST_SIZE_COUNT) ? (uint8_t)cls : (uint8_t)REST_SIZE_NONE;
    const size_t  cap = restChooseStreamSize(kRestPresize[idx], platformMaxFreeBlock(),
                                             RESPONSE_STREAM_BUFFER_SIZE);
    g_restStream = currentRequest->beginResponseStream(contentType, cap);
    if (g_restStream) {
      g_restSizeClass = idx;
      g_restSizeCap   = (uint16_t)cap;
    }
    webApplyHeaders(g_restStream);
  }
  return g_restStream;
}

inline void restFinalize() {
  if (!currentRequest || g_responseSent) return;
  if (g_restStream) {
    const uint8_t cls = g_restSizeClass;
    if (cls != REST_SIZE_NONE && cls < REST_SIZE_COUNT) {
      // available() is the whole body here: nothing has been sent yet.
      restStreamSizeReport(cls, g_restSizeCap, g_restStream->available());
    }
    g_restSizeClass = REST_SIZE_NONE;
    g_restSizeCap   = 0;
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
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*
****************************************************************************
*/
