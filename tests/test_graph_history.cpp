// TASK-1296 - device-side Monitor graph history.
//
// Includes the REAL src/OTGW-firmware/graphHistoryStuff.h and the REAL
// src/OTGW-firmware/jsonEmit.h (only Arduino.h / platform.h are stubbed, see
// tests/stubs). The same checks run for BOTH ring configurations the firmware uses:
//   small (no PSRAM): 126 slots of 30 s,  120 emitted = 1 h
//   large (PSRAM):    726 slots of 120 s, 720 emitted = 24 h
// Covers: ring wrap, commit with gap fill, flag accumulation, encoders (rounding,
// sentinels, NaN), the emit cap, the JSON shape (exact string compare, interval_s per
// config), null for no value, the empty ring, and chunked reassembly over 1460 B windows
// the way restSendChunked does it, with commits landing between windows (the determinism
// contract and the margin: MARGIN commits identical, MARGIN+1 differs).
//
// NOT covered here: the glue in graphHistoryStuff.ino (graphHistoryTick(), sendGraphHistory(),
// the PSRAM-or-static choice in graphHistoryInit()): the presence rule (getMsgLastUpdated), the
// outside-temperature source, the flag OR from the 1 Hz tick, the mutex and the REST handler.
// Those are checked on the bench.
//
// Build (MSVC): cl /std:c++17 /EHsc /I tests\stubs /I src\OTGW-firmware tests\test_graph_history.cpp /Fe:tests\test_graph_history.exe
// Build (g++):  g++ -std=c++17 -I tests/stubs -I src/OTGW-firmware tests/test_graph_history.cpp -o tests/test_graph_history.out
// Run: exit 0 = pass.

#include "Arduino.h"
#include "jsonEmit.h"
#include "graphHistoryStuff.h"

#include <iostream>
#include <string>
#include <vector>
#include <memory>

static int g_failures = 0;
static void check(bool pass, const std::string& what) {
  std::cout << (pass ? "PASS: " : "FAIL: ") << what << "\n";
  if (!pass) g_failures++;
}

struct StringSink : Print {
  std::string s;
  size_t write(uint8_t b) override { s.push_back((char)b); return 1; }
};

// Ring plus its backing storage (a local array stands in for the static array / PSRAM block).
struct RingBox {
  std::vector<GraphHistSlot> mem;
  GraphHistRing r;
  GraphHistClock c;
  RingBox(uint16_t cap, uint16_t ivS) : mem(cap) { graphHistInit(r, c, mem.data(), cap, ivS); }
  void copyStateFrom(const RingBox& o) { mem = o.mem; r.slot = mem.data(); r.head = o.r.head; r.count = o.r.count; r.total = o.r.total; c = o.c; }
};

static GraphHistSlot slotWithFlow(int16_t flow) {
  GraphHistSlot s = graphHistEmptySlot();
  s.flow = flow;
  return s;
}

static std::string emit(const GraphHistRing& r, const GraphHistSnapshot& s) {
  StringSink out;
  JsonEmit je(out);
  graphHistEmitJson(je, r, s);
  return out.s;
}

// Tokens of "key":[...] (text, "null" kept).
static bool column(const std::string& json, const std::string& key, std::vector<std::string>& out) {
  out.clear();
  size_t p = json.find("\"" + key + "\":[");
  if (p == std::string::npos) return false;
  p += key.size() + 4;
  size_t e = json.find(']', p);
  if (e == std::string::npos) return false;
  std::string body = json.substr(p, e - p);
  size_t i = 0;
  while (i < body.size()) {
    size_t c = body.find(',', i);
    if (c == std::string::npos) c = body.size();
    out.push_back(body.substr(i, c - i));
    i = c + 1;
  }
  return true;
}

static bool numberField(const std::string& json, const std::string& key, long& v) {
  size_t p = json.find("\"" + key + "\":");
  if (p == std::string::npos) return false;
  v = std::stol(json.substr(p + key.size() + 3));
  return true;
}

// Copy of RestChunkWindow from src/OTGW-firmware/jsonChunked.h (that header needs ESPAsyncWebServer).
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
  uint8_t* _out; size_t _cap; size_t _start; size_t _pos; size_t _written;
};

// What restSendChunked does: one window per pass, re-running the emit closure from byte 0 each
// time. between(pass) runs after each window, where the loop task could commit slots.
template <class F>
static std::string reassemble(const GraphHistRing& r, const GraphHistSnapshot& snap, F between) {
  std::string out;
  size_t index = 0;
  for (int pass = 0; pass < 1000; pass++) {
    uint8_t buf[1460];
    RestChunkWindow win(buf, sizeof(buf), index);
    JsonEmit je(win);
    graphHistEmitJson(je, r, snap);
    if (win.written() == 0) break;
    out.append((const char*)buf, win.written());
    index += win.written();
    between(pass);
  }
  return out;
}

// Minimal JSON validator for the response shape: objects, arrays, quoted keys, integers, null.
static bool jsonValue(const std::string& s, size_t& i);
static bool jsonValid(const std::string& s) { size_t i = 0; return jsonValue(s, i) && i == s.size(); }
static bool jsonValue(const std::string& s, size_t& i) {
  if (i >= s.size()) return false;
  if (s[i] == '{') {
    i++;
    if (i < s.size() && s[i] == '}') { i++; return true; }
    for (;;) {
      if (i >= s.size() || s[i] != '"') return false;
      i++;
      while (i < s.size() && s[i] != '"') i++;
      if (i >= s.size()) return false;
      i++;
      if (i >= s.size() || s[i] != ':') return false;
      i++;
      if (!jsonValue(s, i)) return false;
      if (i < s.size() && s[i] == ',') { i++; continue; }
      if (i < s.size() && s[i] == '}') { i++; return true; }
      return false;
    }
  }
  if (s[i] == '[') {
    i++;
    if (i < s.size() && s[i] == ']') { i++; return true; }
    for (;;) {
      if (!jsonValue(s, i)) return false;
      if (i < s.size() && s[i] == ',') { i++; continue; }
      if (i < s.size() && s[i] == ']') { i++; return true; }
      return false;
    }
  }
  if (s.compare(i, 4, "null") == 0) { i += 4; return true; }
  size_t st = i;
  if (s[i] == '-') i++;
  size_t d = i;
  while (i < s.size() && s[i] >= '0' && s[i] <= '9') i++;
  return i > d && i > st;
}

static void runCommon() {
  check(sizeof(GraphHistSlot) == 14, "slot is 14 bytes");
  check(GRAPH_HIST_SMALL_SLOTS * sizeof(GraphHistSlot) == 1764, "small ring is 1764 B");
  check(GRAPH_HIST_LARGE_SLOTS * sizeof(GraphHistSlot) == 10164, "large ring is 10164 B");
  check((GRAPH_HIST_SMALL_SLOTS - GRAPH_HIST_MARGIN) * GRAPH_HIST_SMALL_INTERVAL_S == 3600, "small ring emits exactly 1 h");
  check((GRAPH_HIST_LARGE_SLOTS - GRAPH_HIST_MARGIN) * GRAPH_HIST_LARGE_INTERVAL_S == 24 * 3600, "large ring emits exactly 24 h");

  check(graphHistEncTemp(21.54f, true) == 215, "21.54 -> 215");
  check(graphHistEncTemp(21.56f, true) == 216, "21.56 -> 216");
  check(graphHistEncTemp(-3.26f, true) == -33, "-3.26 -> -33 (round half away from zero)");
  check(graphHistEncTemp(0.0f, true) == 0, "0.0 -> 0 (a real zero is not 'no value')");
  check(graphHistEncTemp(50.0f, false) == GRAPH_HIST_NO_TEMP, "not present -> no value");
  check(graphHistEncTemp(NAN, true) == GRAPH_HIST_NO_TEMP, "NaN -> no value");
  check(graphHistEncTemp(INFINITY, true) == GRAPH_HIST_NO_TEMP, "Inf -> no value");
  check(graphHistEncTemp(5000.0f, true) == 32767, "huge value clamps high");
  check(graphHistEncTemp(-5000.0f, true) == -32767, "huge negative clamps above the sentinel");
  check(graphHistEncMod(37.4f, true) == 37, "mod 37.4 -> 37");
  check(graphHistEncMod(37.6f, true) == 38, "mod 37.6 -> 38");
  check(graphHistEncMod(-2.0f, true) == 0, "mod negative -> 0");
  check(graphHistEncMod(180.0f, true) == 100, "mod above 100 -> 100");
  check(graphHistEncMod(NAN, true) == GRAPH_HIST_NO_MOD, "mod NaN -> 255");
  check(graphHistEncMod(10.0f, false) == GRAPH_HIST_NO_MOD, "mod not present -> 255");
  char fb[8];
  check(std::string(graphHistFmtInt(fb, 0)) == "0", "fmt 0");
  check(std::string(graphHistFmtInt(fb, -33)) == "-33", "fmt -33");
  check(std::string(graphHistFmtInt(fb, 32767)) == "32767", "fmt 32767");
  check(std::string(graphHistFmtInt(fb, -32767)) == "-32767", "fmt -32767");
}

static void runConfig(const char* name, uint16_t CAP, uint16_t IVS) {
  const std::string tag = std::string("[") + name + "] ";
  const uint32_t IV = (uint32_t)IVS * 1000UL;
  const int EMIT = CAP - GRAPH_HIST_MARGIN;
  auto ck = [&](bool ok, const std::string& what) { check(ok, tag + what); };

  // ---- clock ------------------------------------------------------------
  {
    RingBox b(CAP, IVS); GraphHistRing& r = b.r; GraphHistClock& c = b.c;
    ck(graphHistIntervalsDue(r, c, 5000) == 0 && c.started && c.slotStartMs == 5000, "first call starts the clock");
    ck(graphHistIntervalsDue(r, c, 5000 + IV - 1) == 0, "not due before one interval");
    ck(graphHistIntervalsDue(r, c, 5000 + IV) == 1, "due at one interval");
    graphHistCommit(r, c, 1, slotWithFlow(215));
    ck(r.count == 1 && r.total == 1 && r.head == 1, "one commit pushes one slot");
    ck(c.slotStartMs == 5000 + IV && c.lastCommitMs == 5000 + IV, "slot start advances by exactly one interval");
    ck(graphHistIntervalsDue(r, c, 5000 + IV + 800) == 0, "jitter does not make the next slot due");
  }

  // ---- gap fill ---------------------------------------------------------
  {
    RingBox b(CAP, IVS); GraphHistRing& r = b.r; GraphHistClock& c = b.c;
    graphHistIntervalsDue(r, c, 0);
    graphHistCommit(r, c, graphHistIntervalsDue(r, c, IV), slotWithFlow(100));
    uint32_t late = IV + 5 * IV + 1234;           // slot started at IV, now 5 intervals + 1.2 s later
    uint32_t k = graphHistIntervalsDue(r, c, late);
    ck(k == 5, "five intervals late -> k = 5");
    graphHistCommit(r, c, k, slotWithFlow(200));
    ck(r.count == 1 + 5 && r.total == 6, "k late pushes k-1 empty slots plus the real one");
    ck(r.slot[1].flow == GRAPH_HIST_NO_TEMP && r.slot[4].flow == GRAPH_HIST_NO_TEMP &&
       r.slot[1].mod == GRAPH_HIST_NO_MOD && r.slot[1].flags == 0, "gap slots hold no values and no flags");
    ck(r.slot[5].flow == 200, "real slot is last");
    ck(c.slotStartMs == 6 * IV, "slot start advanced by k intervals (no drift)");
    uint32_t k2 = graphHistIntervalsDue(r, c, c.slotStartMs + 5000UL * IV);
    graphHistCommit(r, c, k2, slotWithFlow(300));
    ck(r.count == CAP, "huge gap fills at most the ring");
    ck(r.slot[(r.head + CAP - 1) % CAP].flow == 300, "real slot still newest after a capped gap");
    ck(r.total == 6 + (uint32_t)CAP + 1, "total counts the capped gap pushes plus the real slot");
  }

  // ---- flag accumulation ----------------------------------------------
  {
    RingBox b(CAP, IVS); GraphHistRing& r = b.r; GraphHistClock& c = b.c;
    graphHistIntervalsDue(r, c, 0);
    c.flagsAcc |= GRAPH_HIST_FLAG_FLAME;
    c.flagsAcc |= GRAPH_HIST_FLAG_CH;
    c.flagsAcc |= GRAPH_HIST_FLAG_FLAME;
    GraphHistSlot v = graphHistEmptySlot(); v.flags = 0xFF;   // caller flags must be ignored
    graphHistCommit(r, c, graphHistIntervalsDue(r, c, IV), v);
    ck(r.slot[0].flags == (GRAPH_HIST_FLAG_FLAME | GRAPH_HIST_FLAG_CH), "flags are the OR of the interval, not the caller value");
    ck(c.flagsAcc == 0, "accumulator resets after a commit");
    graphHistCommit(r, c, graphHistIntervalsDue(r, c, 2 * IV), graphHistEmptySlot());
    ck(r.slot[1].flags == 0, "next slot starts with clean flags");
  }

  // ---- wrap, snapshot, emit cap ----------------------------------------
  {
    RingBox b(CAP, IVS); GraphHistRing& r = b.r; GraphHistClock& c = b.c;
    graphHistIntervalsDue(r, c, 0);
    for (int i = 1; i <= CAP + 10; i++) {
      graphHistCommit(r, c, graphHistIntervalsDue(r, c, (uint32_t)i * IV), slotWithFlow((int16_t)i));
    }
    ck(r.count == CAP, "count saturates at the capacity");
    ck(r.total == (uint32_t)CAP + 10, "total keeps counting");
    ck(r.head == 10, "head wrapped");
    GraphHistSnapshot s = graphHistMakeSnapshot(r, c, c.lastCommitMs + 7777);
    ck(s.n == EMIT, "snapshot emits capacity minus the margin");
    ck(s.newestAgeMs == 7777, "newest age = now - newest slot time");
    const int newest = CAP + 10;
    const int oldest = newest - (EMIT - 1);
    ck(r.slot[s.start].flow == (int16_t)oldest, "oldest emitted slot is the EMIT-th newest");
    std::string json = emit(r, s);
    std::vector<std::string> flow;
    ck(column(json, "flow", flow) && (int)flow.size() == EMIT, "emit-cap values in the flow column");
    ck(!flow.empty() && flow.front() == std::to_string(oldest) && flow.back() == std::to_string(newest), "flow column runs oldest to newest");
    long cnt = -1; ck(numberField(json, "count", cnt) && cnt == EMIT, "count field is the emit cap");
    long ivj = -1; ck(numberField(json, "interval_s", ivj) && ivj == IVS, "interval_s is this ring's interval");
  }

  // ---- JSON shape on a small fill --------------------------------------
  {
    RingBox b(CAP, IVS); GraphHistRing& r = b.r; GraphHistClock& c = b.c;
    graphHistIntervalsDue(r, c, 1000);
    GraphHistSlot a = graphHistEmptySlot();
    a.flow = 655; a.ret = 301; a.chSet = 700; a.roomSet = 205; a.room = 198; a.outside = -35; a.mod = 42;
    c.flagsAcc = GRAPH_HIST_FLAG_FLAME | GRAPH_HIST_FLAG_CH;
    graphHistCommit(r, c, graphHistIntervalsDue(r, c, 1000 + IV), a);
    graphHistCommit(r, c, graphHistIntervalsDue(r, c, 1000 + 2 * IV), graphHistEmptySlot());
    GraphHistSnapshot s = graphHistMakeSnapshot(r, c, c.lastCommitMs + 1500);
    std::string json = emit(r, s);
    const std::string expected =
      "{\"interval_s\":" + std::to_string(IVS) + ",\"newest_age_ms\":1500,\"count\":2,\"scale\":10,"
      "\"flow\":[655,null],\"ret\":[301,null],\"chSet\":[700,null],\"roomSet\":[205,null],"
      "\"room\":[198,null],\"outside\":[-35,null],\"mod\":[42,null],"
      "\"flame\":[1,0],\"dhw_on\":[0,0],\"ch\":[1,0]}";
    ck(json == expected, "exact JSON for a two-slot ring (integers x10, null for no value, flags 0/1)");
    if (json != expected) std::cout << "  got: " << json << "\n  exp: " << expected << "\n";
    ck(json.find('.') == std::string::npos, "no floats on the wire");
  }

  // ---- empty ring -----------------------------------------------------
  {
    RingBox b(CAP, IVS);
    GraphHistSnapshot s = graphHistMakeSnapshot(b.r, b.c, 123456);
    ck(s.n == 0 && s.newestAgeMs == 0, "empty ring: n = 0, age = 0");
    std::string json = emit(b.r, s);
    const std::string expected =
      "{\"interval_s\":" + std::to_string(IVS) + ",\"newest_age_ms\":0,\"count\":0,\"scale\":10,"
      "\"flow\":[],\"ret\":[],\"chSet\":[],\"roomSet\":[],\"room\":[],\"outside\":[],\"mod\":[],"
      "\"flame\":[],\"dhw_on\":[],\"ch\":[]}";
    ck(json == expected, "count 0 gives empty arrays");
    if (json != expected) std::cout << "  got: " << json << "\n";
  }

  // ---- chunked re-run determinism (restSendChunked) ---------------------
  // Every ~1460 B window re-runs the emit closure from byte 0 and keeps [index, index + 1460).
  // The loop task may push slots between two windows. Margin rule: up to GRAPH_HIST_MARGIN pushed
  // slots during one stream do not change the bytes; one more does. Full wrapped ring, mixed gap
  // and real slots.
  {
    RingBox b(CAP, IVS); GraphHistRing& r = b.r; GraphHistClock& c = b.c;
    graphHistIntervalsDue(r, c, 0);
    uint32_t now = 0;
    const int fill = CAP + 200;
    for (int i = 1; i <= fill; i++) {
      uint32_t step = (i % 17 == 0) ? 4 : 1;                 // a late tick: 3 gap slots + the real one
      now += step * IV;
      GraphHistSlot v = graphHistEmptySlot();
      v.flow = (int16_t)(300 + (i * 37) % 500);
      v.ret = (i % 11 == 0) ? GRAPH_HIST_NO_TEMP : (int16_t)(280 + i % 90);
      v.chSet = (int16_t)(500 + i % 300);
      v.roomSet = (int16_t)(200 + i % 5);
      v.room = (i % 7 == 0) ? GRAPH_HIST_NO_TEMP : (int16_t)(190 + i % 30);
      v.outside = (int16_t)(-40 + i % 60);
      v.mod = (i % 5 == 0) ? GRAPH_HIST_NO_MOD : (uint8_t)(i % 101);
      c.flagsAcc = (uint8_t)(i & 7);
      graphHistCommit(r, c, graphHistIntervalsDue(r, c, now), v);
    }
    ck(r.count == CAP && r.total > CAP, "determinism fixture: ring is full and wrapped");
    int gaps = 0;
    for (int i = 0; i < CAP; i++) if (r.slot[i].flow == GRAPH_HIST_NO_TEMP && r.slot[i].mod == GRAPH_HIST_NO_MOD) gaps++;
    ck(gaps > 0 && gaps < CAP / 2, "determinism fixture mixes gap slots and real slots");

    const GraphHistSnapshot snap = graphHistMakeSnapshot(r, c, c.lastCommitMs + 4321);
    const std::string single = emit(r, snap);
    ck(jsonValid(single), "single-pass body is valid JSON");

    int burstSeed = 0;
    auto commitOne = [&]() {
      GraphHistSlot v = graphHistEmptySlot();
      v.flow = v.ret = v.chSet = v.roomSet = v.room = v.outside = (int16_t)(9000 + ++burstSeed);
      v.mod = 99;
      graphHistPush(r, v);
    };
    RingBox fresh(CAP, IVS);
    fresh.copyStateFrom(b);   // every scenario starts from the same bytes
    auto restore = [&]() { b.copyStateFrom(fresh); };

    restore();
    ck(reassemble(r, snap, [&](int) {}) == single, "windows with no commit reassemble to the single-pass body");
    if (CAP > 200) ck(single.size() > 3 * 1460, "the body spans several windows");

    bool allSame = true, allValid = true;
    for (int m = 1; m <= GRAPH_HIST_MARGIN; m++) {
      restore();
      std::string burst = reassemble(r, snap, [&](int pass) { if (pass == 0) for (int k = 0; k < m; k++) commitOne(); });
      restore();
      std::string spread = reassemble(r, snap, [&](int pass) { if (pass < m) commitOne(); });
      allSame = allSame && burst == single && spread == single;
      allValid = allValid && jsonValid(burst) && jsonValid(spread);
    }
    ck(allSame, "1..MARGIN pushed slots between windows (burst or one per window): body equals the single pass");
    ck(allValid, "and every reassembled body is valid JSON");

    const int over = GRAPH_HIST_MARGIN + 1;
    restore();
    std::string burstOver = reassemble(r, snap, [&](int pass) { if (pass == 0) for (int k = 0; k < over; k++) commitOne(); });
    restore();
    std::string spreadOver = reassemble(r, snap, [&](int pass) { if (pass < over) commitOne(); });
    ck(burstOver != single, "MARGIN+1 pushed slots in a burst change the stream (pins the margin)");
    // With a short body (small ring: about 5 windows) the spread variant lands its last push after the
    // stream ended, so only the burst is pinned there; the large ring pins both.
    if (single.size() > (size_t)(over + 1) * 1460) ck(spreadOver != single, "MARGIN+1 pushed slots spread over the windows change the stream (pins the margin)");
    std::cout << "  (" << name << " body " << single.size() << " bytes in " << (single.size() + 1459) / 1460 << " windows)\n";
  }

  // ---- worst-case body size -------------------------------------------
  {
    RingBox b(CAP, IVS); GraphHistRing& r = b.r; GraphHistClock& c = b.c;
    graphHistIntervalsDue(r, c, 0);
    for (int i = 1; i <= CAP; i++) {
      GraphHistSlot v = graphHistEmptySlot();
      v.flow = 655; v.ret = 301; v.chSet = 700; v.roomSet = 205; v.room = -198; v.outside = -35; v.mod = 100;
      c.flagsAcc = 7;
      graphHistCommit(r, c, graphHistIntervalsDue(r, c, (uint32_t)i * IV), v);
    }
    GraphHistSnapshot s = graphHistMakeSnapshot(r, c, c.lastCommitMs);
    std::cout << "  (" << name << " dense body = " << emit(r, s).size() << " bytes)\n";
  }
}

int main() {
  runCommon();
  runConfig("small 126x30s", GRAPH_HIST_SMALL_SLOTS, GRAPH_HIST_SMALL_INTERVAL_S);
  runConfig("large 726x120s", GRAPH_HIST_LARGE_SLOTS, GRAPH_HIST_LARGE_INTERVAL_S);
  std::cout << (g_failures ? "=== FAILED ===\n" : "=== ALL TESTS PASSED ===\n") << "failures=" << g_failures << "\n";
  return g_failures ? 1 : 0;
}
