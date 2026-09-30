// test/host/test_diagnose_handler.cpp  (TASK-1133 AC#1)
//
// Runs the REAL diagnose branch of the /api/v2/otgw handler (restAPI.ino) with
// the REAL JSON scanner (jsonStuff.ino), both sliced by test_diagnose_handler.py.
// Only I/O is doubled: the request body accessor, the HTTP answers and the PIC
// transmit queue record what the branch did.

#include "arduino_shim.h"
#include <vector>

static int                  g_status = 0;       // HTTP status the branch answered with
static std::string          g_message;          // its error message, if any
static std::vector<uint8_t> g_pic;              // every byte handed to the PIC transmit queue
static const char*          g_body = "";

static void sendApiMethodNotAllowed(const __FlashStringHelper*) { g_status = 405; }
static void sendApiError(int code, const __FlashStringHelper* message) {
  g_status = code; g_message = reinterpret_cast<const char*>(message);
}
static bool isPICEnabled() { return true; }
static bool isDiagnoseFirmware() { return true; }
static const char* bodyCompat() { return g_body; }
static bool enqueuePICTx(const uint8_t* bytes, size_t len) { g_pic.insert(g_pic.end(), bytes, bytes + len); return true; }
static void sendCorsOriginHeader() {}
static void webSend(int code, const __FlashStringHelper*, const char*) { g_status = code; }

#include "diagnose_scanner.inc"                  // extractJsonField() and its helpers

// The branch body runs inside this function; its 'return' statements end it.
static void diagnoseBranch(bool isPostOrPut) {
#include "diagnose_branch.inc"
}

static int g_failures = 0;
static std::string shown(const std::vector<uint8_t>& v) {
  std::string s;
  for (uint8_t b : v) {
    if (b == '\r') s += "\\r"; else if (b == '\n') s += "\\n"; else s += static_cast<char>(b);
  }
  return s.empty() ? "-" : s;
}
static void run(const char* id, const char* title, const char* body, int wantStatus, const char* wantBytes) {
  g_status = 0; g_message.clear(); g_pic.clear(); g_body = body;
  diagnoseBranch(true);
  const std::string got = shown(g_pic);
  const bool ok = g_status == wantStatus && got == wantBytes;
  std::printf("CASE %s %s\n     %s\n     got : status=%d to_pic='%s'%s%s\n", id, ok ? "pass" : "FAIL", title,
              g_status, got.c_str(), g_message.empty() ? "" : " message=", g_message.c_str());
  if (!ok) std::printf("     want: status=%d to_pic='%s'\n", wantStatus, wantBytes);
  if (!ok) g_failures++;
}

int main() {
  std::printf("== POST /api/v2/otgw/diagnose, the real branch (TASK-1133 AC#1) ==\n");
  run("H1", "{\"input\":\"\\r\"} (the reported body, no data field): 400, nothing to the PIC",
      "{\"input\":\"\\r\"}", 400, "-");
  run("H2", "control: an empty body: 400, nothing to the PIC", "", 400, "-");
  run("H3", "a bare text body '1<CR>' (no JSON): 400, nothing to the PIC", "1\r", 400, "-");
  run("H4", "data longer than 32 characters: 400, nothing to the PIC",
      "{\"data\":\"123456789012345678901234567890123\"}", 400, "-");
  run("H5", "control: the web UI body {\"data\":\"1\\r\"}: 202, '1'+CR to the PIC",
      "{\"data\":\"1\\r\"}", 202, "1\\r");
  run("H6", "control: {\"data\":\"\\n\"}, nothing sendable after the filter: 400, nothing to the PIC",
      "{\"data\":\"\\n\"}", 400, "-");
  run("H7", "{} (an object without data): 400, nothing to the PIC", "{}", 400, "-");
  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
