// TASK-1565 host harness: does a request read the PREVIOUS request's body?
// The body-capture code (webServerCompat.h: WebRequestBody, webCaptureBody,
// hasBodyCompat, bodyCompat) is included from the generated slice, never copied.
// The mock below stands in for ESPAsyncWebServer only: a request object whose
// destructor runs its onDisconnect handler, as AsyncWebServerRequest does, and
// whose onDisconnect keeps ONE callback: a second call REPLACES the first
// (ESPAsyncWebServer WebRequest.cpp: _onDisconnectfn = fn).
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <new>

typedef std::function<void()> ArDisconnectHandler;
struct AsyncWebServerRequest {
  size_t _len;
  ArDisconnectHandler _fn;
  explicit AsyncWebServerRequest(size_t contentLength) : _len(contentLength) {}
  ~AsyncWebServerRequest() { if (_fn) _fn(); }
  size_t contentLength() const { return _len; }
  void onDisconnect(ArDisconnectHandler fn) { _fn = fn; }
};
static AsyncWebServerRequest* currentRequest = nullptr;
static bool g_restSlotHeld = false;
static int restReleases = 0, fileReleases = 0;
void restSlotRelease() { restReleases++; }
void webFileGateRelease() { fileReleases++; }

#include "body_slice.inc"
WebRequestBody g_requestBody{};

static int failures = 0;
static void check(const char* id, bool ok, const char* what) {
  printf("CASE %s %s  %s\n", id, ok ? "pass" : "FAIL", what);
  if (!ok) failures++;
}
static void capture(AsyncWebServerRequest* r, const char* body) {
  webCaptureBody(r, (const uint8_t*)body, strlen(body), 0, strlen(body));
}

// One fixed storage block so request B lands on request A's address, the
// condition the allocator produces on the device.
alignas(AsyncWebServerRequest) static unsigned char slot[sizeof(AsyncWebServerRequest)];

int main() {
  // S1 control: a request with a captured body reads its own body.
  AsyncWebServerRequest* a = new (slot) AsyncWebServerRequest(40);
  capture(a, "{\"name\":\"sattargettemp\",\"value\":18.5}");
  currentRequest = a;
  check("S1", hasBodyCompat() && strstr(bodyCompat(), "18.5") != nullptr, "own captured body is visible");

  // S2 defect: A ends, B (form-urlencoded, hook never called) is created at the
  // same address with a non-zero Content-Length. It must not see A's body.
  a->~AsyncWebServerRequest();
  AsyncWebServerRequest* b = new (slot) AsyncWebServerRequest(3);
  currentRequest = b;
  check("S2", !hasBodyCompat() && bodyCompat()[0] == '\0', "same-address request without a capture sees no body");

  // S3 control: B with its own capture reads its own body, not A's.
  capture(b, "pwm");
  check("S3", hasBodyCompat() && strcmp(bodyCompat(), "pwm") == 0, "request with its own capture reads its own body");

  // S4 control: an older request that is destroyed late must not wipe the body
  // of a newer owner (keep-alive: B is current while an earlier request ends).
  AsyncWebServerRequest older(5);
  capture(&older, "older");
  capture(b, "newer");
  currentRequest = b;
  older.~AsyncWebServerRequest();
  new (&older) AsyncWebServerRequest(0);   // keep the automatic destructor valid
  check("S4", hasBodyCompat() && strcmp(bodyCompat(), "newer") == 0, "late end of another request keeps the current body");

  b->~AsyncWebServerRequest();
  currentRequest = nullptr;

  // S5 defect (bench): body captured, then the REST gate is taken and
  // webArmSlotRelease() runs. Its callback must still release the body, and the
  // gate slot exactly once.
  restReleases = fileReleases = 0;
  a = new (slot) AsyncWebServerRequest(40);
  capture(a, "{\"name\":\"sattargettemp\",\"value\":18.5}");
  currentRequest = a; g_restSlotHeld = true;
  webArmSlotRelease();
  a->~AsyncWebServerRequest();
  check("S5", restReleases == 1 && fileReleases == 0 && g_requestBody.owner == nullptr,
        "capture then gate arm: gate released once and body released");
  b = new (slot) AsyncWebServerRequest(3);
  currentRequest = b; g_restSlotHeld = false;
  check("S5b", !hasBodyCompat() && bodyCompat()[0] == '\0', "next request at the same address sees no body");
  b->~AsyncWebServerRequest();

  // S6: rearming replaces the callback, never duplicates REST/body cleanup.
  restReleases = fileReleases = 0;
  a = new (slot) AsyncWebServerRequest(40);
  capture(a, "body6");
  currentRequest = a; g_restSlotHeld = true;
  webArmSlotRelease();
  webArmSlotRelease();
  a->~AsyncWebServerRequest();
  check("S6", restReleases == 1 && fileReleases == 0 && g_requestBody.owner == nullptr,
        "rearmed REST/body callback runs once; file release stays response-owned");

  // S7: the gate is taken (and armed) BEFORE the body capture; the capture must
  // not lose the gate release.
  restReleases = fileReleases = 0;
  a = new (slot) AsyncWebServerRequest(40);
  currentRequest = a; g_restSlotHeld = true;
  webArmSlotRelease();
  capture(a, "body7");
  a->~AsyncWebServerRequest();
  check("S7", restReleases == 1 && g_requestBody.owner == nullptr,
        "gate armed before capture: gate still released once and body released");
  currentRequest = nullptr; g_restSlotHeld = false;

  printf(failures ? "FAILED %d\n" : "ALL PASS\n", failures);
  return failures ? 1 : 0;
}
