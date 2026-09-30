// test/host/test_raw_passthrough.cpp  (TASK-1111 AC#1)
//
// Runs the REAL PIC byte path sliced by test_raw_passthrough.py: the task side
// picSerialDrainOnce() (with enqueueOTFrame()) and the loop side
// drainOTFrameQueue() (with drainOTRawQueue() where the revision has it). The
// UART, the value queues, the clock and OTGWstream are doubles; OTGWstream
// records every byte with the time it arrived.

#include "arduino_shim.h"
#include <deque>
#include <vector>

#define HAS_PIC 1
#define DebugTf(...) ((void)0)

// ---- doubles -------------------------------------------------------------------------
static uint32_t g_now = 0;                        // the firmware clock, advanced by the driver
static uint32_t millis() { return g_now; }

// A value-copy queue of fixed-size items, as platformQueue* is on the device.
struct HostQueue { size_t item; size_t depth; std::deque<std::vector<uint8_t>> items; };
typedef HostQueue* PlatformQueue;
static PlatformQueue platformQueueCreate(size_t depth, size_t item) { return new HostQueue{ item, depth, {} }; }
static bool platformQueueSend(PlatformQueue q, const void* p) {
  if (!q || q->items.size() >= q->depth) return false;
  const uint8_t* b = static_cast<const uint8_t*>(p);
  q->items.emplace_back(b, b + q->item);
  return true;
}
static bool platformQueueSendToFront(PlatformQueue q, const void* p) {
  if (!q || q->items.size() >= q->depth) return false;
  const uint8_t* b = static_cast<const uint8_t*>(p);
  q->items.emplace_front(b, b + q->item);
  return true;
}
static bool platformQueueReceive(PlatformQueue q, void* p, uint32_t = 0) {
  if (!q || q->items.empty()) return false;
  std::memcpy(p, q->items.front().data(), q->item);
  q->items.pop_front();
  return true;
}

// The PIC UART: bytes the PIC has sent and the UART holds.
struct UartDouble {
  std::deque<uint8_t> rx;
  int available() { return static_cast<int>(rx.size()); }
  int read() { if (rx.empty()) return -1; int b = rx.front(); rx.pop_front(); return b; }
  int availableForWrite() { return 256; }
  size_t write(const uint8_t*, size_t n) { return n; }
  void flush() {}
} OTGWSerial;
static bool platformSerialHasOverrun(UartDouble&) { return false; }
static bool platformSerialHasRxError(UartDouble&) { return false; }

// Port 25238: every byte a client receives, with its arrival time.
struct StreamDouble {
  std::vector<uint8_t> out; std::vector<uint32_t> at;
  size_t write(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) { out.push_back(b[i]); at.push_back(g_now); } return n; }
  size_t write(uint8_t b) { out.push_back(b); at.push_back(g_now); return 1; }
} OTGWstream;

static struct { struct { bool bLegacyPort25238Enabled; } mqtt; } settings;
static bool isDiagnoseFirmware() { return false; }
static void forwardDiagnoseChunk(const uint8_t*, uint8_t) {}
static void feedWatchDog() {}
static const int LED2 = 0;
static void blinkLEDnow(int) {}
static void processOT(const char*, int, bool) {}
static void reportPendingPICRxErrors() {}

// ---- code under test, sliced ------------------------------------------------------------
#include "raw_passthrough.inc"

// ---- driver -------------------------------------------------------------------------------
static void tick() { g_now += 2; picSerialDrainOnce(); drainOTFrameQueue(); }   // task every 2 ms, loop after it

static std::string shown(const std::vector<uint8_t>& v, size_t maxLen = 60) {
  std::string s;
  for (size_t i = 0; i < v.size() && s.size() < maxLen; i++) {
    const uint8_t b = v[i];
    if (b == '\r') s += "\\r"; else if (b == '\n') s += "\\n";
    else if (b < 0x20 || b > 0x7E) { char h[8]; std::snprintf(h, sizeof(h), "\\x%02X", b); s += h; }
    else s += static_cast<char>(b);
  }
  if (v.size() > 0 && s.size() >= maxLen) s += "...";
  return s.empty() ? "-" : s + " (" + std::to_string(v.size()) + " bytes)";
}

static int g_failures = 0;
// Sends `input` at 9600 baud (one byte per ms), then idles 100 ms. Passes when
// OTGWstream received exactly the input; with maxLatencyMs >= 0 the last byte
// must also arrive within that many ms of the PIC sending it.
static void run(const char* id, const char* title, const std::vector<uint8_t>& input, int maxLatencyMs = -1) {
  OTGWstream.out.clear(); OTGWstream.at.clear();
  size_t pos = 0;
  uint32_t lastSentAt = 0;
  while (pos < input.size()) {
    for (int k = 0; k < 2 && pos < input.size(); k++) { OTGWSerial.rx.push_back(input[pos++]); lastSentAt = g_now + 1; }
    tick();
  }
  for (int i = 0; i < 50; i++) tick();
  bool ok = OTGWstream.out == input;
  int latency = -1;
  if (ok && !OTGWstream.at.empty()) latency = static_cast<int>(OTGWstream.at.back() - lastSentAt);
  if (ok && maxLatencyMs >= 0) ok = latency >= 0 && latency <= maxLatencyMs;
  std::printf("CASE %s %s\n     %s\n     sent: %s\n     got : %s", id, ok ? "pass" : "FAIL", title,
              shown(input).c_str(), shown(OTGWstream.out).c_str());
  if (maxLatencyMs >= 0) std::printf("  latency %d ms (max %d)", latency, maxLatencyMs);
  std::printf("\n");
  if (!ok) g_failures++;
}

static std::vector<uint8_t> bytes(const char* s, size_t n) { return std::vector<uint8_t>(s, s + n); }
static std::vector<uint8_t> text(const char* s) { return bytes(s, std::strlen(s)); }

int main() {
  std::printf("== PIC bytes to port 25238, the real task and loop path (TASK-1111 AC#1) ==\n");
  settings.mqtt.bLegacyPort25238Enabled = true;
  otFrameQueue = platformQueueCreate(8, sizeof(OTFrameMsg));
  otTxQueue    = platformQueueCreate(8, sizeof(OTTxMsg));
#ifdef HARNESS_HAS_RAW_PATH
  otRawQueue   = platformQueueCreate(OT_RAW_QUEUE_DEPTH, sizeof(OTRawMsg));
#endif

  run("P1", "control: two CRLF-terminated OT lines arrive byte for byte",
      text("T80000200\r\nB40000200\r\n"));
  run("P3", "a line ended by a bare LF arrives as sent, no CR synthesised",
      text("E: LF only\n"));
  run("P4", "an empty line between two lines is forwarded, not swallowed",
      text("A1\r\n\r\nB2\r\n"));
  {
    std::string longLine(600, 'x');
    longLine += "\r\n";
    run("P5", "a 600-byte line (over MAX_BUFFER_READ) still reaches the client; only the parser drops it",
        text(longLine.c_str()));
  }
  run("P6", "control: NUL and 0xFF inside a CRLF-terminated line arrive unmodified",
      bytes("N\x00\xFFZ\r\n", 6));
  run("P2", "a prompt without a terminator ('Enter test number: ') arrives within the coalescing window",
      text("Enter test number: "), 40);

  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
