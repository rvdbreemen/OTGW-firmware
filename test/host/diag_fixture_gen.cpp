//=======================================================================
// diag_fixture_gen.cpp  (TASK-1667)
//
// Writes a diagnostic recording with the REAL encoder (src/OTGW-firmware/diagRecorderStuff.h):
// a PSRAM recorder takes a known event sequence (every record type, repeats, a dropped record
// that becomes a Gap record, a text over 254 bytes, dt above 16383 ms, several blocks), then the
// download is built the way the firmware route does it (diagDownloadRange, the file header with
// diagDownloadHeaderBegin/End and the route names, diagDownloadFill chunk by chunk).
//
//   diag_fixture_gen <recording.bin> <expected.txt>
//
// expected.txt holds one "<uptime ms>\t<line>" per record, in the canonical form that
// scripts/diag_decode.py fmt() prints. tests/test_diag_decode.py compares the two.
//=======================================================================

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "diagRecorderStuff.h"

static uint32_t g_ms = 1000, g_epoch = 1760000000u;
static bool g_lockBusy = false;
static uint32_t eMillis() { return g_ms; }
static uint32_t eEpoch() { return g_epoch; }
static uint32_t eFree() { return 4000000u; }
static void* eAlloc(size_t n) { return std::calloc(1, n); }
static void eFreeBlk(void* p) { std::free(p); }
static void* ePsram(size_t n) { return std::calloc(1, n); }
static bool eTry() { return !g_lockBusy; }
static bool eWait(uint32_t) { return true; }
static void eUnlock() {}

static std::vector<std::string> g_lines;
static void want(const std::string& s) { char t[16]; std::snprintf(t, sizeof t, "%u\t", (unsigned)g_ms); g_lines.push_back(t + s); }
static std::string hex(uint32_t v, int w) { char b[16]; std::snprintf(b, sizeof b, "%0*x", w, (unsigned)v); return b; }

int main(int argc, char** argv) {
  if (argc != 3) { std::fprintf(stderr, "usage: %s recording.bin expected.txt\n", argv[0]); return 2; }
  DiagEnv e;
  e.millis = eMillis; e.epoch = eEpoch; e.freeHeap = eFree; e.allocHeap = eAlloc; e.freeHeapBlock = eFreeBlk;
  e.allocPsramOnce = ePsram; e.tryLock = eTry; e.lockWait = eWait; e.unlock = eUnlock; e.hasPsram = true;
  DiagRecorder* r = new DiagRecorder();
  if (r->start(e) != DIAG_START_OK) { std::fprintf(stderr, "start failed\n"); return 2; }

  // every record type once
  r->recReboot(7, "Software reset", "none"); want("REBOOT 7 Software reset | none");
  g_ms += 5; r->recOtFrame('T', 0x80190000u); want("OT0 80190000");
  g_ms += 7; r->recOtFrame('B', 0xC0191B80u); want("OT1 c0191b80");
  g_ms += 1; r->recOtFrame('R', 0x80000100u); want("OT2 80000100");
  g_ms += 1; r->recOtFrame('A', 0xC0000300u); want("OT3 c0000300");
  g_ms += 1; r->recOtFrame('E', 0x00000000u); want("OT4 00000000");
  g_ms += 9; r->recPicCommand("PR=A", 4); want("PIC PR=A");
  g_ms += 3; r->recOtdCommand("TT=21.5", 7); want("OTD TT=21.5");
  g_ms += 3; r->recResponse("TT: 21.50", 9); want("RSP TT: 21.50");
  g_ms += 1; r->recMqttPublish("boiler_flow_temp", "45.5"); want("MQ " + hex(diagNameId("boiler_flow_temp"), 4) + " 45.5");
  g_ms += 1; r->recMqttIn("set/otgw/command", "HW=1", 4); want("MQIN set/otgw/command HW=1");
  g_ms += 1; r->recRestIn(1, 2, "{\"cmd\":\"GW=1\"}", 14); want("REST 1 2 {\"cmd\":\"GW=1\"}");
  g_ms += 1; r->recWeather(1234, 5600, 310); want("WX 1234 5600 310");
  g_ms += 1; r->recBle(2, 2150, 4400, 87, -71); want("BLE 2 2150 4400 87 -71");
  g_ms += 1; r->recSetting("SATtargettemp", "20.5"); want("SET " + hex(diagNameId("SATtargettemp"), 4) + " 20.5");
  g_ms += 1; r->recTimeSync(1760000123u); want("TIME 1760000123");

  // a text over 254 bytes is cut and flagged
  std::string longCmd(300, 'X');
  g_ms += 2; r->recPicCommand(longCmd.c_str(), longCmd.size()); want("PIC " + std::string(254, 'X') + " <cut>");

  // dt above 16383 ms (a 3-byte varint) and a repeat of the same frame
  g_ms += 20000; r->recOtFrame('B', 0xC0191B80u); want("OT1 c0191b80");

  // one record dropped on a busy lock: the next record is preceded by a Gap record
  g_lockBusy = true; g_ms += 4; r->recOtFrame('T', 0x00000300u);
  g_lockBusy = false; g_ms += 4;
  want("GAP 1"); r->recOtFrame('T', 0x00000300u); want("OT0 00000300");

  // enough traffic for several blocks: the bus rhythm with repeats and changing values
  for (uint32_t i = 0; i < 3000; i++) {
    g_ms += 3;
    const uint32_t id = i % 20;
    const uint32_t val = (i / 60) & 0xFF;              // changes every 60 events: mostly repeats
    const char src = "TB"[i & 1];
    const uint32_t frame = ((src == 'B') ? 0x40000000u : 0x00000000u) | (id << 16) | val;
    r->recOtFrame(src, frame); want(std::string("OT") + (src == 'B' ? "1 " : "0 ") + hex(frame, 8));
    if (i % 7 == 0) {
      char topic[16]; std::snprintf(topic, sizeof topic, "t%u", (unsigned)(i % 5));
      char value[16]; std::snprintf(value, sizeof value, "%u", (unsigned)((i / 70) % 4));
      g_ms += 1; r->recMqttPublish(topic, value); want("MQ " + hex(diagNameId(topic), 4) + " " + value);
    }
  }

  // the download, built like the firmware route
  DiagDownload d;
  if (!diagDownloadRange(*r, d, false, 0) || d.count < 3) { std::fprintf(stderr, "range: %u blocks\n", (unsigned)d.count); return 2; }
  DiagHdrWriter w(d);
  diagDownloadHeaderBegin(w, d, *r, 7, true, "2.0.0-test", "abc1234");
  const char* routes[] = { "settings", "command" };
  w.u8(2); w.str(routes[0]); w.str(routes[1]);
  if (!diagDownloadHeaderEnd(w, d)) { std::fprintf(stderr, "header too long\n"); return 2; }

  FILE* f = std::fopen(argv[1], "wb");
  if (!f) return 2;
  uint8_t buf[1460];
  size_t index = 0;
  for (;;) {
    size_t n = diagDownloadFill(*r, d, buf, sizeof buf, index);
    if (n == 0) break;
    if (n == DIAG_FILL_TRY_AGAIN) { std::fprintf(stderr, "lock busy\n"); return 2; }
    std::fwrite(buf, 1, n, f);
    index += n;
  }
  std::fclose(f);

  FILE* x = std::fopen(argv[2], "wb");
  if (!x) return 2;
  for (auto& s : g_lines) std::fprintf(x, "%s\n", s.c_str());
  std::fclose(x);
  std::printf("blocks %u, records %u, bytes %u\n", (unsigned)d.count, (unsigned)g_lines.size(), (unsigned)index);
  return 0;
}
