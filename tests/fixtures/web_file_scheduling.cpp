// Platform/transport fakes only: the public compat header and gate are production code.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
static bool failNextNothrow = false;
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  if (failNextNothrow) { failNextNothrow = false; return nullptr; }
  try { return ::operator new(size); } catch (...) { return nullptr; }
}

struct __FlashStringHelper;
using PGM_P = const char*;
#define F(s) reinterpret_cast<const __FlashStringHelper*>(s)
#define PSTR(s) s
#define FPSTR(s) reinterpret_cast<const __FlashStringHelper*>(s)
#define PROGMEM
#define RESTDebugTf(...) do {} while (0)
#define strcmp_P strcmp
#define strlen_P strlen
static size_t hostStrlcpy(char* dest, const char* src, size_t cap) {
  const size_t len = strlen(src);
  if (cap) { const size_t take = std::min(cap - 1, len); memcpy(dest, src, take); dest[take] = 0; }
  return len;
}
#define strlcpy hostStrlcpy
#define strlcpy_P hostStrlcpy
class String : public std::string {
public:
  using std::string::string;
  String(const __FlashStringHelper* value) : std::string(reinterpret_cast<const char*>(value)) {}
};
static uint32_t clockMs = 0, maxBlock = 32000;
static uint16_t wsClients = 0;
enum { ESTABLISHED, FIN_WAIT_1 };
struct tcp_pcb {
  tcp_pcb* next = nullptr;
  uint16_t local_port = 80;
  int state = ESTABLISHED;
  void* callback_arg = nullptr;
  void* unsent = nullptr;
  void* unacked = nullptr;
};
static tcp_pcb* tcp_active_pcbs = nullptr;
static bool coreLocked = false, trackClients = false;
#define LOCK_TCPIP_CORE() do { if (coreLocked) std::abort(); coreLocked = true; } while (0)
#define UNLOCK_TCPIP_CORE() do { if (!coreLocked) std::abort(); coreLocked = false; } while (0)
static void linkPcb(tcp_pcb* pcb) { pcb->next = tcp_active_pcbs; tcp_active_pcbs = pcb; }
static void unlinkPcb(tcp_pcb* pcb) {
  for (auto** slot = &tcp_active_pcbs; *slot; slot = &(*slot)->next) {
    if (*slot == pcb) { *slot = pcb->next; pcb->next = nullptr; return; }
  }
}
static tcp_pcb blockers[6];
static void setSenders(unsigned count) {
  for (auto& pcb : blockers) unlinkPcb(&pcb);
  for (unsigned i = 0; i < count; ++i) {
    blockers[i] = {}; blockers[i].unsent = &blockers[i]; linkPcb(&blockers[i]);
  }
}
uint32_t millis() { return clockMs; }
uint32_t platformMaxFreeBlock() { return maxBlock; }
uint32_t platformFreeHeap() { return 64000; }
#include "tcp_gate.inc"
uint16_t webSocketClientCount() { return wsClients; }
#include "heap_diag.inc"
struct { HeapDiagSection heapdiag; } state;

struct AsyncWebHeader {
  String key, val;
  AsyncWebHeader(const String& name, const String& value) : key(name), val(value) {}
  const String& name() const { return key; }
  const String& value() const { return val; }
};
struct AsyncWebParameter { String value() const { return {}; } };
class AsyncWebServerRequest;
enum WebResponseState { RESPONSE_SETUP, RESPONSE_HEADERS, RESPONSE_CONTENT, RESPONSE_WAIT_ACK, RESPONSE_END, RESPONSE_FAILED };
class AsyncWebServerResponse {
protected:
  int _code = 0;
  std::list<AsyncWebHeader> _headers;
  String _contentType;
  WebResponseState _state = RESPONSE_SETUP;
public:
  virtual ~AsyncWebServerResponse() = default;
  int code() const { return _code; }
  void setCode(int value) { _code = value; }
  bool addHeader(const String& name, const String& value, bool replace = true) {
    if (replace) _headers.remove_if([&](const AsyncWebHeader& h) { return h.name() == name; });
    _headers.emplace_back(name, value); return true;
  }
  bool addHeader(const AsyncWebHeader& header, bool replace = true) { return addHeader(header.name(), header.value(), replace); }
  const std::list<AsyncWebHeader>& getHeaders() const { return _headers; }
  virtual bool _started() const { return _state > RESPONSE_SETUP; }
  virtual bool _finished() const { return _state > RESPONSE_WAIT_ACK; }
  virtual bool _failed() const { return _state == RESPONSE_FAILED; }
  virtual bool _sourceValid() const { return false; }
  virtual void _respond(AsyncWebServerRequest*) { _state = RESPONSE_END; }
  virtual size_t _ack(AsyncWebServerRequest*, size_t, uint32_t) { return 0; }
  const String& type() const { return _contentType; }
};
struct FileEntry { bool valid = true; bool compressed = false; };
struct FakeFS {
  std::map<std::string, FileEntry> files;
  unsigned opens = 0, openNow = 0, openHwm = 0;
  mutable unsigned existsChecks = 0;
  bool exists(const String& path) const { ++existsChecks; return files.count(path) != 0; }
} LittleFS;
struct AsyncClient {
  tcp_pcb control;
  AsyncClient() { control.callback_arg = this; if (trackClients) linkPcb(&control); }
  ~AsyncClient() { unlinkPcb(&control); }
  tcp_pcb* pcb() { return &control; }
  bool writable = true, aborted = false;
  uint32_t rxTimeout = 0, lastRx = clockMs;
  void onTimeout(std::function<void(void*, AsyncClient*, uint32_t)>, void*) {}
  bool canSend() const { return writable; }
  void setRxTimeout(uint32_t seconds) { rxTimeout = seconds; }
  uint32_t getRxTimeout() const { return rxTimeout; }
  void abort() { aborted = true; }
};
struct Wire {
  int status = 0;
  std::string type, path;
  std::map<std::string, std::string> headers;
};
class BoundaryResponse : public AsyncWebServerResponse {
  bool valid, file;
  std::string path;
public:
  BoundaryResponse(int code, const String& type, bool valid = true, bool file = false, const String& path = {})
      : valid(valid), file(file), path(path) { _code = code; _contentType = type; }
  ~BoundaryResponse() override { if (file) --LittleFS.openNow; }
  bool _sourceValid() const override { return valid; }
  void _respond(AsyncWebServerRequest*) override;
  size_t _ack(AsyncWebServerRequest*, size_t, uint32_t) override { _state = RESPONSE_END; return 0; }
};
class AsyncResponseStream : public BoundaryResponse {
public:
  AsyncResponseStream() : BoundaryResponse(200, "application/json") {}
};
#define RESPONSE_STREAM_BUFFER_SIZE 1460
namespace AsyncWebRequestMethod { enum Method { HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE, HTTP_PATCH, HTTP_HEAD, HTTP_OPTIONS }; }
enum HTTPMethod { HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE, HTTP_PATCH, HTTP_HEAD, HTTP_OPTIONS, HTTP_ANY };
struct AsyncWebServer {};
static unsigned nullBasicResponses = 0, nullFileResponses = 0;
class AsyncWebServerRequest {
  AsyncClient transport;
  std::function<void()> disconnect;
  bool sent = false;
public:
  AsyncWebServerResponse* response = nullptr;
  Wire wire;
  ~AsyncWebServerRequest() { delete response; }
  AsyncClient* client() { return &transport; }
  void onDisconnect(std::function<void()> fn) { disconnect = std::move(fn); }
  void abort() { if (!sent) { sent = true; transport.abort(); } }
  void cancel() {
    unlinkPcb(transport.pcb());
    if (disconnect) { auto fn = std::move(disconnect); disconnect = nullptr; fn(); }
    delete response; response = nullptr;
  }
  void start() {
    if (sent) { if (transport.aborted) cancel(); return; }
    if (!response) response = beginResponse(501, "text/plain", "Handler did not handle the request");
    if (!response->_sourceValid()) { delete response; response = beginResponse(500, "text/plain", "Invalid data in handler"); }
    transport.setRxTimeout(0);
    response->_respond(this);
    sent = true;
    if (transport.aborted) cancel();
  }
  void poll() {
    // Native AsyncTCP checks RX inactivity before the request's canSend-gated poll.
    if (transport.rxTimeout && uint32_t(clockMs - transport.lastRx) >= transport.rxTimeout * 1000U) { cancel(); return; }
    if (response && client()->canSend()) response->_ack(this, 0, 0);
    if (transport.aborted) cancel();
  }
  void complete() { response->_ack(this, 1, 0); if (response->_finished()) cancel(); }
  void send(AsyncWebServerResponse* value) { delete response; response = value; }
  AsyncWebServerResponse* beginResponse(int code, const String& type = {}, const String& = {}) {
    if (nullBasicResponses) { --nullBasicResponses; return nullptr; }
    return new BoundaryResponse(code, type);
  }
  AsyncWebServerResponse* beginResponse(int code, const String& type, const uint8_t*, size_t) { return new BoundaryResponse(code, type); }
  AsyncWebServerResponse* beginResponse(FakeFS& fs, const String& path, const char* type) {
    if (nullFileResponses) { --nullFileResponses; return nullptr; }
    if (!fs.exists(path)) return nullptr;
    ++fs.opens; ++fs.openNow; fs.openHwm = std::max(fs.openNow, fs.openHwm);
    auto* result = new BoundaryResponse(200, type, fs.files.at(path).valid, true, path);
    result->addHeader("Content-Disposition", "inline");
    if (fs.files.at(path).compressed) result->addHeader("Content-Encoding", "gzip");
    return result;
  }
  AsyncResponseStream* beginResponseStream(const char*, size_t) { return new AsyncResponseStream; }
  const AsyncWebParameter* getParam(const char*, bool) const { return nullptr; }
  bool hasParam(const char*, bool) const { return false; }
  const AsyncWebHeader* getHeader(const char*) const { return nullptr; }
  bool hasHeader(const char*) const { return false; }
  String host() const { return {}; }
  String url() const { return {}; }
  size_t contentLength() const { return 0; }
  AsyncWebRequestMethod::Method method() const { return AsyncWebRequestMethod::HTTP_GET; }
  bool authenticate(const char*, const char*) const { return true; }
  void requestAuthentication() {}
};
void BoundaryResponse::_respond(AsyncWebServerRequest* req) {
  req->wire.status = _code; req->wire.type = _contentType; req->wire.path = path;
  if (trackClients) req->client()->pcb()->unacked = req->client();
  for (const auto& h : _headers) req->wire.headers[h.name()] = h.value();
  _state = RESPONSE_WAIT_ACK;
}

#include "webServerCompat.h"
AsyncWebServer server;
AsyncWebServerRequest* currentRequest = nullptr;
AsyncResponseStream* g_restStream = nullptr;
bool g_responseSent = false, g_restSlotHeld = false;
WebPendingHeaders g_pendingHeaders{};
WebRequestBody g_requestBody{};
static uint8_t restInFlight = 0;
#include "file_gate.inc"

void sampleHeapWatermark() {}
#include "reset_heap.inc"
static int failures = 0;
#define EXPECT(condition) do { if (!(condition)) { std::printf("FAIL %s:%d: %s\n", __func__, __LINE__, #condition); ++failures; } } while (0)
static std::unique_ptr<AsyncWebServerRequest> sendFile(const char* path = "/asset.css", bool rest = false, bool gzip = false) {
  auto request = std::make_unique<AsyncWebServerRequest>();
  webBeginRequest(request.get());
  if (rest) { ++restInFlight; g_restSlotHeld = true; webArmSlotRelease(); }
  const uint8_t body[] = {'x'};
  webCaptureBody(request.get(), body, sizeof(body), 0, sizeof(body));
  webPushHeader(F("ETag"), F("\"build-asset\""));
  webPushHeader(F("Cache-Control"), F("no-cache"));
  webSendFile(path, F("text/css"), gzip);
  request->start();
  return request;
}
static void reset() {
  EXPECT(webFileInFlight == 0); EXPECT(restInFlight == 0); EXPECT(LittleFS.openNow == 0);
  LittleFS = {}; LittleFS.files["/asset.css"] = {}; LittleFS.files["/other.css"] = {};
  clockMs = 0; maxBlock = 32000; wsClients = 0; state.heapdiag = {};
  setSenders(0); trackClients = false;
}
static void waits_then_streams_and_preserves_headers() {
  reset(); auto first = sendFile(); auto second = sendFile();
  const unsigned opens = LittleFS.opens;
  const unsigned existsChecks = LittleFS.existsChecks;
  auto third = sendFile("/other.css", true, true);
  EXPECT(third->wire.status == 0); EXPECT(LittleFS.opens == opens); EXPECT(webFileInFlight == 2);
  for (unsigned i = 0; i < 20; ++i) third->poll();
  EXPECT(LittleFS.existsChecks == existsChecks);
  EXPECT(state.heapdiag.iWebfile503Count == 0); EXPECT(state.heapdiag.iWebConn503Count == 0);
  first->cancel(); third->poll();
  EXPECT(third->wire.status == 200); EXPECT(third->wire.path == "/other.css"); EXPECT(third->wire.type == "text/css");
  EXPECT(third->wire.headers["ETag"] == "\"build-asset\""); EXPECT(third->wire.headers["Cache-Control"] == "no-cache");
  EXPECT(third->wire.headers["Content-Encoding"] == "gzip"); EXPECT(third->wire.headers["Content-Disposition"] == "inline");
  EXPECT(webFileInFlight == 2); EXPECT(restInFlight == 1); EXPECT(LittleFS.openHwm == 2);
  third->complete(); EXPECT(webFileInFlight == 1); EXPECT(restInFlight == 0); EXPECT(g_requestBody.owner == nullptr);
  second->cancel(); third->cancel(); EXPECT(webFileInFlight == 0);
}
static void fifo_queue_cap_and_cancellation() {
  reset(); auto first = sendFile(); auto second = sendFile();
  auto a = sendFile(); auto b = sendFile("/asset.css", true); auto c = sendFile(); auto d = sendFile();
  EXPECT(a->wire.status == 0); EXPECT(d->wire.status == 0); EXPECT(LittleFS.opens == 2);
  auto overflow = sendFile(); EXPECT(overflow->wire.status == 503); EXPECT(overflow->wire.headers["Retry-After"] == "1");
  EXPECT(overflow->wire.headers.count("ETag") == 0); EXPECT(overflow->wire.headers["Cache-Control"] == "no-store");
  EXPECT(state.heapdiag.iWebfile503Count == 1);
  b->cancel(); EXPECT(restInFlight == 0); auto e = sendFile(); EXPECT(e->wire.status == 0);
  first->cancel(); e->poll(); d->poll(); c->poll(); EXPECT(c->wire.status == 0); EXPECT(e->wire.status == 0);
  // Free-slot pumping must preserve FIFO even before native polls arrive.
  auto full = sendFile();
  EXPECT(a->wire.status == 200); EXPECT(full->wire.status == 0);
  a->cancel(); c->poll(); EXPECT(c->wire.status == 200);
  c->cancel(); d->poll(); EXPECT(d->wire.status == 200);
  d->cancel(); e->poll(); EXPECT(e->wire.status == 200);
  e->cancel(); full->poll(); EXPECT(full->wire.status == 200);
  full->cancel(); second->cancel(); overflow->cancel();
}
static void low_memory_deadline_wrap_and_safety() {
  reset(); maxBlock = 15000; clockMs = UINT32_MAX - 5000;
  auto first = sendFile(); auto wait = sendFile(); EXPECT(webFileInFlight == 1); EXPECT(wait->wire.status == 0);
  clockMs += 9999; wait->poll(); EXPECT(wait->wire.status == 0);
  clockMs += 1; wait->poll(); EXPECT(wait->wire.status == 503); EXPECT(wait->wire.headers["Retry-After"] == "1");
  EXPECT(wait->wire.headers.count("ETag") == 0); EXPECT(wait->wire.headers["Cache-Control"] == "no-store");
  EXPECT(webFileInFlight == 1); EXPECT(state.heapdiag.iWebfile503Count == 1);
  auto next = sendFile(); first->cancel(); next->poll(); EXPECT(next->wire.status == 200); EXPECT(LittleFS.openHwm == 1);
  next->cancel(); wait->cancel();
  setSenders(WEB_MAX_TX_CONNECTIONS + 1);
  auto refused = sendFile(); EXPECT(refused->wire.status == 0);
  for (unsigned i = 0; i < 20; ++i) refused->poll();
  EXPECT(state.heapdiag.iWebConn503Count == 0);
  clockMs += 10000; refused->poll(); EXPECT(refused->wire.status == 503);
  EXPECT(webFileInFlight == 0); refused->cancel();
  setSenders(0); auto busy = sendFile(); auto pending = sendFile();
  setSenders(WEB_MAX_TX_CONNECTIONS + 1); pending->poll();
  EXPECT(pending->wire.status == 0);
  clockMs += 10000; pending->poll(); EXPECT(pending->wire.status == 503);
  EXPECT(state.heapdiag.iWebConn503Count == 0);
  EXPECT(pending->wire.headers.count("ETag") == 0);
  pending->cancel(); busy->cancel();
}
static void errors_bounds_and_response_ownership() {
  reset(); auto missing = sendFile("/missing.css"); EXPECT(missing->wire.status == 404); missing->cancel();
  LittleFS.files["/bad.css"] = {false, false};
  auto invalid = sendFile("/bad.css"); EXPECT(invalid->wire.status == 500); invalid->cancel(); EXPECT(webFileInFlight == 0);
  // Deferred metadata must be rejected, not silently shortened into another resource/type.
  std::string longPath(256, 'x'); longPath[0] = '/';
  const unsigned opens = LittleFS.opens;
  auto tooLong = sendFile(longPath.c_str()); EXPECT(tooLong->wire.status == 414); EXPECT(LittleFS.opens == opens); tooLong->cancel();
  auto mime = std::make_unique<AsyncWebServerRequest>(); webBeginRequest(mime.get());
  std::string longMime(128, 'm'); webSendFile("/asset.css", longMime.c_str(), false); mime->start();
  EXPECT(mime->wire.status == 400); EXPECT(LittleFS.opens == opens); mime->cancel();
  auto flashMime = std::make_unique<AsyncWebServerRequest>(); webBeginRequest(flashMime.get());
  webSendFile("/asset.css", reinterpret_cast<const __FlashStringHelper*>(longMime.c_str()), false); flashMime->start();
  EXPECT(flashMime->wire.status == 400); flashMime->cancel();
  // Response destruction, even without onDisconnect, must return its own file slot.
  { auto owned = sendFile(); EXPECT(webFileInFlight == 1); }
  EXPECT(webFileInFlight == 0); EXPECT(LittleFS.openNow == 0);
  auto first = sendFile(); auto second = sendFile();
  { auto pending = sendFile(); EXPECT(pending->wire.status == 0); }
  first->cancel(); second->cancel(); auto after = sendFile(); EXPECT(after->wire.status == 200); after->cancel();
}
static void deferred_metadata_and_file_changes() {
  reset(); auto first = sendFile(); auto second = sendFile();
  char path[] = "/other.css";
  auto queued = sendFile(path);
  path[1] = 'X';  // Request-local path storage may be reused before admission.
  first->cancel(); queued->poll();
  EXPECT(queued->wire.status == 200); EXPECT(queued->wire.path == "/other.css");
  auto removed = sendFile("/other.css");
  LittleFS.files.erase("/other.css");
  second->cancel(); removed->poll();
  EXPECT(removed->wire.status == 404); EXPECT(removed->wire.headers.count("ETag") == 0);
  EXPECT(removed->wire.headers["Cache-Control"] == "no-store");
  EXPECT(removed->wire.headers.count("Content-Encoding") == 0);
  auto invalid = sendFile("/asset.css");
  auto invalidQueued = sendFile("/asset.css");
  LittleFS.files["/asset.css"].valid = false;
  invalid->cancel(); invalidQueued->poll();
  EXPECT(invalidQueued->wire.status == 500); EXPECT(invalidQueued->wire.headers.count("ETag") == 0);
  EXPECT(invalidQueued->wire.headers["Cache-Control"] == "no-store");
  removed->cancel(); invalidQueued->cancel(); queued->cancel();
}
static void unwritable_pending_and_null_responses() {
  reset(); auto first = sendFile(); auto second = sendFile();
  auto waiting = sendFile("/asset.css", true);
  waiting->client()->writable = false;
  clockMs = 10999; waiting->poll(); EXPECT(waiting->response != nullptr); EXPECT(restInFlight == 1);
  clockMs = 11000; waiting->poll();
  EXPECT(waiting->response == nullptr); EXPECT(restInFlight == 0); EXPECT(webFileInFlight == 2);
  auto recovered = sendFile();
  first->cancel(); recovered->poll(); EXPECT(recovered->wire.status == 200);
  EXPECT(recovered->client()->getRxTimeout() == 0);
  recovered->cancel(); second->cancel();

  reset(); nullFileResponses = 1;
  auto fileFailure = sendFile(); EXPECT(fileFailure->wire.status == 500); EXPECT(webFileInFlight == 0); fileFailure->cancel();
  nullFileResponses = nullBasicResponses = 1;
  auto fallbackFailure = sendFile("/asset.css", true);
  EXPECT(fallbackFailure->client()->aborted); EXPECT(fallbackFailure->response == nullptr);
  EXPECT(webFileInFlight == 0); EXPECT(restInFlight == 0);
  failNextNothrow = true;
  auto wrapperFailure = sendFile("/asset.css", true);
  EXPECT(wrapperFailure->client()->aborted); EXPECT(wrapperFailure->response == nullptr);
  EXPECT(wrapperFailure->wire.status == 0);
  EXPECT(webFileInFlight == 0); EXPECT(restInFlight == 0);
  auto active = sendFile(); auto other = sendFile();
  failNextNothrow = true;
  auto pendingAllocationFailure = sendFile("/asset.css", true);
  EXPECT(pendingAllocationFailure->client()->aborted); EXPECT(restInFlight == 0);
  EXPECT(pendingAllocationFailure->wire.status == 0);
  auto timedOut = sendFile("/asset.css", true);
  clockMs += 10000; nullBasicResponses = 1; timedOut->poll();
  EXPECT(timedOut->client()->aborted); EXPECT(timedOut->response == nullptr);
  EXPECT(webFileInFlight == 2); EXPECT(restInFlight == 0);
  active->cancel(); other->cancel();
}
static void six_native_connections_progress_without_polling() {
  reset(); trackClients = true;
  std::vector<std::unique_ptr<AsyncWebServerRequest>> requests;
  for (unsigned i = 0; i < 6; ++i) requests.emplace_back(new AsyncWebServerRequest);
  for (auto& request : requests) {
    webBeginRequest(request.get()); webSendFile("/asset.css", F("text/css"), false); request->start();
    EXPECT(request->wire.status != 503);
    EXPECT(state.heapdiag.iWebfilePending <= 4);
  }
  EXPECT(requests[0]->wire.status == 200); EXPECT(requests[1]->wire.status == 200);
  EXPECT(state.heapdiag.iWebfilePending == 4);
  for (unsigned i = 0; i < 6; ++i) {
    for (auto& request : requests) request->poll();
    EXPECT(requests[i]->wire.status == 200);
    for (unsigned j = i + 2; j < 6; ++j) EXPECT(requests[j]->wire.status == 0);
    requests[i]->complete();
  }
  EXPECT(state.heapdiag.iWebfileInflightHwm == 2); EXPECT(LittleFS.openHwm == 2);
  EXPECT(state.heapdiag.iWebfilePending == 0); EXPECT(state.heapdiag.iWebfilePendingHwm == 4);
  EXPECT(state.heapdiag.iWebfile503Count == 0); EXPECT(state.heapdiag.iWebConn503Count == 0);
}
static void pending_identity_never_hides_tx_or_unrelated_pcbs() {
  reset(); trackClients = true; maxBlock = 15000;
  auto first = sendFile(); auto pending = sendFile();
  setSenders(4);
  EXPECT(webConnCapExceeded());
  pending->client()->pcb()->unsent = pending.get();
  const PlatformTcpIdleConnection excluded[] = {{pending->client()->pcb(), pending->client()}};
  EXPECT(platformTcpTxHoldingOnPort(80, excluded, 1) == 6);
  pending->client()->pcb()->state = FIN_WAIT_1;
  EXPECT(platformTcpTxHoldingOnPort(80, excluded, 1) == 6);
  pending->client()->pcb()->unsent = nullptr;
  pending->client()->pcb()->unacked = pending.get();
  EXPECT(platformTcpTxHoldingOnPort(80, excluded, 1) == 6);
  unlinkPcb(pending->client()->pcb());
  EXPECT(platformTcpTxHoldingOnPort(80, excluded, 1) == 5);
  // Reusing an old PCB address for another sender must not subtract that sender.
  pending->client()->pcb()->state = ESTABLISHED;
  pending->client()->pcb()->unsent = pending->client()->pcb()->unacked = nullptr;
  pending->client()->pcb()->callback_arg = first->client();
  linkPcb(pending->client()->pcb());
  EXPECT(platformTcpTxHoldingOnPort(80, excluded, 1) == 6);
  pending->client()->pcb()->callback_arg = pending->client();
  EXPECT(platformTcpTxHoldingOnPort(80, excluded, 1) == 5);
  pending->cancel(); first->cancel(); setSenders(0);
}
static void measured_queue_lifecycle_and_reset() {
  reset(); auto first = sendFile(); auto second = sendFile();
  EXPECT(state.heapdiag.iWebfileQueued == 0);
  clockMs = UINT32_MAX - 50;
  auto admitted = sendFile(); auto cancelled = sendFile(); auto timedOut = sendFile();
  EXPECT(state.heapdiag.iWebfileQueued == 3); EXPECT(state.heapdiag.iWebfilePending == 3);
  for (unsigned i = 0; i < 20; ++i) admitted->poll();
  EXPECT(state.heapdiag.iWebfileQueued == 3);
  clockMs += 100; cancelled->cancel();
  EXPECT(state.heapdiag.iWebfileCancelled == 1); EXPECT(state.heapdiag.iWebfileMaxWaitMs == 100);
  first->cancel(); admitted->poll(); EXPECT(admitted->wire.status == 200);
  EXPECT(state.heapdiag.iWebfileAdmitted == 1); EXPECT(state.heapdiag.iWebfilePending == 1);
  resetHeapWatermark();
  EXPECT(state.heapdiag.iWebfileQueued == 0); EXPECT(state.heapdiag.iWebfileCancelled == 0);
  EXPECT(state.heapdiag.iWebfilePending == 1); EXPECT(state.heapdiag.iWebfilePendingHwm == 1);
  clockMs += 9900; timedOut->poll();
  EXPECT(timedOut->wire.status == 503); EXPECT(state.heapdiag.iWebfileTimeout == 1);
  EXPECT(state.heapdiag.iWebfilePending == 0); EXPECT(state.heapdiag.iWebfileMaxWaitMs == 10000);
  timedOut->poll(); timedOut->cancel();
  EXPECT(state.heapdiag.iWebfileTimeout == 1); EXPECT(state.heapdiag.iWebfileCancelled == 0);
  admitted->cancel(); second->cancel();
}
int main() {
  waits_then_streams_and_preserves_headers(); fifo_queue_cap_and_cancellation();
  low_memory_deadline_wrap_and_safety(); errors_bounds_and_response_ownership(); deferred_metadata_and_file_changes(); reset();
  unwritable_pending_and_null_responses(); reset();
  six_native_connections_progress_without_polling();
  pending_identity_never_hides_tx_or_unrelated_pcbs(); measured_queue_lifecycle_and_reset(); reset();
  std::printf("%s: production webSendFile scheduling (%d failures)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
