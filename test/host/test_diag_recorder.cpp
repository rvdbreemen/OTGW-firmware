//=======================================================================
// test_diag_recorder.cpp
//
// TASK-1665 host harness, driven by test_diag_recorder.py. It includes the REAL
// src/OTGW-firmware/diagRecorderStuff.h (the encoder, the ring, the start rule and the
// redaction helpers). Only the environment is doubled: clock, heap numbers, allocators
// and the lock (struct DiagEnv). The decoder below is test code (the host decoder of
// TASK-1667 is separate); it decodes ONE block at a time with empty caches, so a repeat
// record whose key is not in that block's own history is a decode error.
//
// Cases (CASE <id> pass|FAIL):
//   F1  every record type round-trips (PSRAM ring)
//   F2  100000 mixed events: each block decodes alone and matches the generator
//   F3  after 400000 events the ring overwrote its oldest blocks: seq/first_record_seq are
//       consecutive from oldest to newest, the overwritten block is gone, survivors decode
//   F4  repeat records: an identical OT frame / MQTT publish is a repeat inside a block and a
//       full record again in the next block
//   F5  lock contention drops and counts: the next record is preceded by a Gap record
//   F6  no-PSRAM: nothing allocated until start, one 4 KB-class block while recording, freed on stop
//   F7  refuse rule: free heap after the allocation below 5120 B refuses with a reason and
//       allocates nothing; exactly 5120 B left starts; an allocation failure refuses
//   F8  PSRAM: platformPsramAllocOnce() path used once over two cycles, heap never touched, nothing freed
//   F9  resume: the first record is the Reboot record; a tight heap refuses the resume
//   F10 redaction: secret settings, location 0.1 degree, secret keys in bodies
//   F11 text over 254 bytes is cut and flagged
//   F12 not recording: a capture point takes no lock and writes nothing
//   F13 dt above 127 ms and above 16383 ms
//=======================================================================

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <map>

#include "diagRecorderStuff.h"

static int g_fail = 0;
static void caseResult(const char* id, bool ok, const std::string& why = "") {
  std::printf("CASE %s %s%s%s\n", id, ok ? "pass" : "FAIL", ok || why.empty() ? "" : "  -- ", ok ? "" : why.c_str());
  if (!ok) g_fail++;
}

// ---- environment doubles ---------------------------------------------------
static uint32_t g_ms = 1000, g_epoch = 0, g_free = 60000;
static int g_heapAllocs = 0, g_heapFrees = 0, g_psramAllocs = 0, g_lockTries = 0;
static bool g_failAlloc = false, g_lockBusy = false, g_psramAvail = true;
static uint32_t g_lastAllocSize = 0;

static uint32_t eMillis() { return g_ms; }
static uint32_t eEpoch() { return g_epoch; }
static uint32_t eFree() { return g_free; }
static void* eAllocHeap(size_t n) { g_heapAllocs++; g_lastAllocSize = (uint32_t)n; return g_failAlloc ? nullptr : std::calloc(1, n); }
static void eFreeHeap(void* p) { g_heapFrees++; std::free(p); }
static void* ePsram(size_t n) { g_psramAllocs++; return g_psramAvail ? std::calloc(1, n) : nullptr; }
static bool eTry() { g_lockTries++; return !g_lockBusy; }
static bool eWait(uint32_t) { return true; }
static void eUnlock() {}

static DiagEnv makeEnv(bool psram) {
  DiagEnv e;
  e.millis = eMillis; e.epoch = eEpoch; e.freeHeap = eFree; e.allocHeap = eAllocHeap; e.freeHeapBlock = eFreeHeap;
  e.allocPsramOnce = ePsram; e.tryLock = eTry; e.lockWait = eWait; e.unlock = eUnlock; e.hasPsram = psram;
  return e;
}
static void resetDoubles() {
  g_ms = 1000; g_epoch = 0; g_free = 60000; g_heapAllocs = g_heapFrees = g_psramAllocs = g_lockTries = 0;
  g_failAlloc = false; g_lockBusy = false; g_psramAvail = true;
}

// ---- test decoder: one block, empty caches ----------------------------------
struct Dec { uint32_t seq, uptime, epoch, firstRec; std::vector<std::string> recs; std::vector<uint32_t> times; std::string err; };

static uint32_t rdU32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static std::string hex(uint32_t v, int w) { char b[16]; std::snprintf(b, sizeof b, "%0*x", w, v); return b; }

static Dec decodeBlock(const uint8_t* b) {
  Dec d{};
  d.seq = rdU32(b); d.uptime = rdU32(b + 4); d.epoch = rdU32(b + 8); d.firstRec = rdU32(b + 12);
  std::map<int, uint32_t> otLast;          // (src<<8|id) -> frame, this block only
  std::map<int, std::string> mqLast;       // topic id -> value, this block only
  size_t i = DIAG_REC_BLOCK_HDR;
  uint32_t t = d.uptime;
  auto text = [&](std::string& out, bool& cut) {
    uint8_t n = b[i++]; cut = (n == DIAG_REC_TEXT_CUT); size_t m = cut ? DIAG_REC_TEXT_MAX : n;
    out.assign((const char*)b + i, m); i += m;
  };
  while (i < DIAG_REC_BLOCK_SIZE && b[i] != DIAG_T_END) {
    uint8_t type = b[i++];
    uint32_t dt = 0; int sh = 0; uint8_t c;
    do { c = b[i++]; dt |= (uint32_t)(c & 0x7F) << sh; sh += 7; } while (c & 0x80);
    t += dt;
    std::string s, s2; bool cut = false, cut2 = false;
    if (type >= DIAG_T_OT_FRAME && type < DIAG_T_OT_FRAME + 5) {
      int src = type - DIAG_T_OT_FRAME;
      uint32_t f = ((uint32_t)b[i] << 24) | (b[i + 1] << 16) | (b[i + 2] << 8) | b[i + 3]; i += 4;
      otLast[(src << 8) | ((f >> 16) & 0xFF)] = f;
      s = "OT" + std::to_string(src) + " " + hex(f, 8);
    } else if (type >= DIAG_T_OT_REPEAT && type < DIAG_T_OT_REPEAT + 5) {
      int src = type - DIAG_T_OT_REPEAT; uint8_t id = b[i++];
      auto it = otLast.find((src << 8) | id);
      if (it == otLast.end()) { d.err = "OT repeat without a base record in this block"; return d; }
      s = "OT" + std::to_string(src) + " " + hex(it->second, 8);
    } else switch (type) {
      case DIAG_T_PIC_CMD: text(s2, cut); s = "PIC " + s2; break;
      case DIAG_T_OTD_CMD: text(s2, cut); s = "OTD " + s2; break;
      case DIAG_T_RESPONSE: text(s2, cut); s = "RSP " + s2; break;
      case DIAG_T_MQTT_PUB: { int id = b[i] | (b[i + 1] << 8); i += 2; text(s2, cut); mqLast[id] = s2; s = "MQ " + hex(id, 4) + " " + s2 + (cut ? "~" : ""); break; }
      case DIAG_T_MQTT_REPEAT: { int id = b[i] | (b[i + 1] << 8); i += 2;
        auto it = mqLast.find(id); if (it == mqLast.end()) { d.err = "MQTT repeat without a base record in this block"; return d; }
        s = "MQ " + hex(id, 4) + " " + it->second; break; }
      case DIAG_T_MQTT_IN: text(s, cut); text(s2, cut2); s = "MQIN " + s + " " + s2 + (cut2 ? "~" : ""); break;
      case DIAG_T_REST_IN: { int rid = b[i], m = b[i + 1]; i += 2; text(s2, cut); s = "REST " + std::to_string(rid) + " " + std::to_string(m) + " " + s2; break; }
      case DIAG_T_WEATHER: { int16_t a = (int16_t)(b[i] | (b[i + 1] << 8)), h = (int16_t)(b[i + 2] | (b[i + 3] << 8)), w = (int16_t)(b[i + 4] | (b[i + 5] << 8)); i += 6;
        s = "WX " + std::to_string(a) + " " + std::to_string(h) + " " + std::to_string(w); break; }
      case DIAG_T_BLE: { int slot = b[i]; int16_t a = (int16_t)(b[i + 1] | (b[i + 2] << 8)), h = (int16_t)(b[i + 3] | (b[i + 4] << 8)); int bat = b[i + 5]; int rssi = (int8_t)b[i + 6]; i += 7;
        s = "BLE " + std::to_string(slot) + " " + std::to_string(a) + " " + std::to_string(h) + " " + std::to_string(bat) + " " + std::to_string(rssi); break; }
      case DIAG_T_SETTING: { int id = b[i] | (b[i + 1] << 8); i += 2; text(s2, cut); s = "SET " + hex(id, 4) + " " + s2; break; }
      case DIAG_T_REBOOT: { uint32_t bc = rdU32(b + i); i += 4; text(s, cut); text(s2, cut2); s = "REBOOT " + std::to_string(bc) + " " + s + " | " + s2; break; }
      case DIAG_T_GAP: { uint32_t n = 0; int sh2 = 0; uint8_t c2; do { c2 = b[i++]; n |= (uint32_t)(c2 & 0x7F) << sh2; sh2 += 7; } while (c2 & 0x80); s = "GAP " + std::to_string(n); break; }
      case DIAG_T_TIME_SYNC: s = "TIME " + std::to_string(rdU32(b + i)); i += 4; break;
      default: d.err = "unknown type " + std::to_string(type); return d;
    }
    d.recs.push_back(s); d.times.push_back(t);
  }
  return d;
}

// ---- generator: event i -> call on the recorder and the expected canonical string -----
static std::string applyEvent(DiagRecorder& r, uint32_t i) {
  const uint32_t k = i % 7;
  if (k == 0 || k == 3) {                                   // OT frame; every second one repeats the previous value
    const int src = (int)((i / 7) % 4); const uint8_t id = (uint8_t)((i / 7) % 6);
    const uint32_t val = ((uint32_t)id << 16) | (((i / 14) % 3 == 0) ? 0x1234u : (uint32_t)(i & 0xFFFFu));
    const uint32_t frame = (src == 0 ? 0x40000000u : 0x10000000u) | val;
    r.recOtFrame("TBRA"[src], frame);
    return "OT" + std::to_string(src) + " " + hex(frame, 8);
  }
  if (k == 1 || k == 4) {                                   // MQTT publish; value changes every 5th event
    char topic[24], val[24];
    std::snprintf(topic, sizeof topic, "boiler/temp%u", (unsigned)((i / 7) % 9));
    std::snprintf(val, sizeof val, "%u.%u", (unsigned)(i / 35), 0u);
    r.recMqttPublish(topic, val);
    return "MQ " + hex(diagNameId(topic), 4) + " " + val;
  }
  if (k == 2) { char c[16]; std::snprintf(c, sizeof c, "TT=%u", (unsigned)(i % 50)); r.recPicCommand(c, std::strlen(c)); return std::string("PIC ") + c; }
  if (k == 5) { char c[16]; std::snprintf(c, sizeof c, "TT: %u", (unsigned)(i % 50)); r.recResponse(c, std::strlen(c)); return std::string("RSP ") + c; }
  { char c[16]; std::snprintf(c, sizeof c, "SB=%u", (unsigned)(i % 9)); r.recOtdCommand(c, std::strlen(c)); return std::string("OTD ") + c; }
}

static DiagRecorder* newPsramRecorder(DiagEnv& e) {
  DiagRecorder* r = new DiagRecorder();
  DiagStartResult s = r->start(e);
  if (s != DIAG_START_OK) { std::printf("start failed %d\n", (int)s); std::exit(2); }
  return r;
}

static void freePsram(DiagRecorder* r) { std::free(r->mem); delete r; }

// -------------------------------------------------------------------------------
int main() {
  DiagEnv ps = makeEnv(true);
  DiagEnv hp = makeEnv(false);

  // F1 ---------------------------------------------------------------------
  {
    resetDoubles(); g_epoch = 1760000000;
    DiagRecorder* r = newPsramRecorder(ps);
    std::vector<std::string> want; std::vector<uint32_t> at;
    auto step = [&](uint32_t dt) { g_ms += dt; at.push_back(g_ms); };
    step(0); r->recReboot(7, "Software reset", "none"); want.push_back("REBOOT 7 Software reset | none");
    step(5); r->recOtFrame('T', 0x40190A00u); want.push_back("OT0 40190a00");
    step(7); r->recOtFrame('E', 0x00000000u); want.push_back("OT4 00000000");
    step(9); r->recPicCommand("PR=A", 4); want.push_back("PIC PR=A");
    step(3); r->recOtdCommand("TT=21.5", 7); want.push_back("OTD TT=21.5");
    step(3); r->recResponse("TT: 21.50", 9); want.push_back("RSP TT: 21.50");
    step(1); r->recMqttPublish("boiler_flow_temp", "45.5"); want.push_back("MQ " + hex(diagNameId("boiler_flow_temp"), 4) + " 45.5");
    step(1); r->recMqttIn("set/otgw-firmware/command", "TT=20", 5); want.push_back("MQIN set/otgw-firmware/command TT=20");
    step(1); r->recRestIn(12, 1, "{\"cmd\":\"GW=1\"}", 14); want.push_back("REST 12 1 {\"cmd\":\"GW=1\"}");
    step(1); r->recWeather(1234, 5600, 310); want.push_back("WX 1234 5600 310");
    step(1); r->recBle(2, 2150, 4400, 87, -71); want.push_back("BLE 2 2150 4400 87 -71");
    step(1); r->recSetting("SATtargettemp", "20.5"); want.push_back("SET " + hex(diagNameId("SATtargettemp"), 4) + " 20.5");
    step(1); r->recTimeSync(1760000123u); want.push_back("TIME 1760000123");
    bool ok = r->hasData() && r->newestSeq() == 0;
    Dec d = decodeBlock(r->block(0));
    ok = ok && d.err.empty() && d.recs == want && d.times == at && d.seq == 0 && d.epoch == 1760000000u && d.firstRec == 0;
    std::string why = d.err;
    if (d.recs != want) { why += " got:"; for (auto& s : d.recs) why += " [" + s + "]"; }
    caseResult("F1", ok, why);
    freePsram(r);
  }

  // F2 / F3 -----------------------------------------------------------------
  {
    resetDoubles(); g_epoch = 1760000000;
    DiagRecorder* r = newPsramRecorder(ps);
    std::vector<std::string> want; std::vector<uint32_t> at;
    const uint32_t N = 100000;
    for (uint32_t i = 0; i < N; i++) { g_ms += (i % 50) * 3 + 1; want.push_back(applyEvent(*r, i)); at.push_back(g_ms); }
    bool ok = true; std::string why; uint32_t checked = 0, blocks = 0;
    for (uint32_t s = r->oldestSeq(); s <= r->newestSeq(); s++) {
      const uint8_t* b = r->block(s);
      if (!b) { ok = false; why = "block missing"; break; }
      Dec d = decodeBlock(b);                                  // fresh caches: block-independent
      if (!d.err.empty()) { ok = false; why = "block " + std::to_string(s) + ": " + d.err; break; }
      for (size_t n = 0; n < d.recs.size(); n++) {
        uint32_t gi = d.firstRec + (uint32_t)n;
        if (gi >= N || d.recs[n] != want[gi] || d.times[n] != at[gi]) { ok = false; why = "block " + std::to_string(s) + " record " + std::to_string(n) + " got [" + d.recs[n] + "] want [" + (gi < N ? want[gi] : "?") + "]"; break; }
        checked++;
      }
      if (!ok) break;
      blocks++;
    }
    caseResult("F2", ok && checked == N && blocks > 5, why + " checked=" + std::to_string(checked) + " blocks=" + std::to_string(blocks));
    // keep going to overflow the ring
    for (uint32_t i = N; i < 400000; i++) { g_ms += (i % 50) * 3 + 1; want.push_back(applyEvent(*r, i)); at.push_back(g_ms); }
    const uint32_t oldest = r->oldestSeq(), newest = r->newestSeq();
    ok = (newest + 1u - oldest == DIAG_REC_PSRAM_BLOCKS) && oldest > 0 && r->block(oldest - 1) == nullptr && r->block(newest + 1) == nullptr;
    why = "oldest=" + std::to_string(oldest) + " newest=" + std::to_string(newest);
    uint32_t prevFirst = 0, nrec = 0;
    for (uint32_t s = oldest; ok && s <= newest; s++) {
      Dec d = decodeBlock(r->block(s));
      if (!d.err.empty() || d.seq != s) { ok = false; why = "block " + std::to_string(s) + " header seq " + std::to_string(d.seq) + " " + d.err; break; }
      if (s > oldest && d.firstRec != prevFirst) { ok = false; why = "first_record_seq not contiguous at " + std::to_string(s); break; }
      for (size_t n = 0; n < d.recs.size(); n++) {
        uint32_t gi = d.firstRec + (uint32_t)n;
        if (d.recs[n] != want[gi]) { ok = false; why = "survivor mismatch at block " + std::to_string(s); break; }
      }
      prevFirst = d.firstRec + (uint32_t)d.recs.size(); nrec += (uint32_t)d.recs.size();
    }
    caseResult("F3", ok, why + " records=" + std::to_string(nrec));
    freePsram(r);
  }

  // F4 ---------------------------------------------------------------------
  {
    resetDoubles();
    DiagRecorder* r = newPsramRecorder(ps);
    r->recOtFrame('T', 0x40010000u); r->recOtFrame('T', 0x40010000u); r->recOtFrame('B', 0x40010000u);
    r->recMqttPublish("a/b", "1"); r->recMqttPublish("a/b", "1"); r->recMqttPublish("a/b", "2");
    const uint8_t* b0 = r->block(0);
    // walk raw: expect types OT_FRAME(T), OT_REPEAT(T), OT_FRAME(B), MQTT_PUB, MQTT_REPEAT, MQTT_PUB
    std::vector<int> types; size_t i = DIAG_REC_BLOCK_HDR;
    while (b0[i] != 0) {
      types.push_back(b0[i]); uint8_t t = b0[i++]; while (b0[i++] & 0x80) {}
      if (t >= 1 && t <= 5) i += 4; else if (t >= 6 && t <= 10) i += 1;
      else if (t == DIAG_T_MQTT_PUB) { i += 2; i += 1 + b0[i]; } else if (t == DIAG_T_MQTT_REPEAT) i += 2;
    }
    std::vector<int> want = { 1, 6, 2, DIAG_T_MQTT_PUB, DIAG_T_MQTT_REPEAT, DIAG_T_MQTT_PUB };
    bool ok = (types == want);
    // fill until the next block opens, then the same frame must be a FULL record again
    uint32_t guard = 0;
    while (r->newestSeq() == 0 && guard++ < 100000) r->recPicCommand("PR=A", 4);
    r->recOtFrame('T', 0x40010000u);
    Dec d1 = decodeBlock(r->block(1));
    ok = ok && r->newestSeq() == 1 && d1.err.empty() && !d1.recs.empty() && d1.recs.back() == "OT0 40010000";
    // and the raw type of that record in block 1 is OT_FRAME (not a repeat)
    const uint8_t* b1 = r->block(1); size_t j = DIAG_REC_BLOCK_HDR, lastStart = j;
    while (b1[j] != 0) { lastStart = j; uint8_t t = b1[j++]; while (b1[j++] & 0x80) {} if (t == DIAG_T_PIC_CMD) j += 1 + b1[j]; else if (t >= 1 && t <= 5) j += 4; }
    ok = ok && b1[lastStart] == DIAG_T_OT_FRAME;
    caseResult("F4", ok);
    freePsram(r);
  }

  // F5 ---------------------------------------------------------------------
  {
    resetDoubles();
    DiagRecorder* r = newPsramRecorder(ps);
    r->recPicCommand("A=1", 3);
    g_lockBusy = true;
    for (int i = 0; i < 5; i++) r->recPicCommand("DROPPED", 7);
    g_lockBusy = false;
    r->recPicCommand("B=2", 3);
    Dec d = decodeBlock(r->block(0));
    bool ok = d.err.empty() && d.recs.size() == 3 && d.recs[0] == "PIC A=1" && d.recs[1] == "GAP 5" && d.recs[2] == "PIC B=2" && r->droppedTotal == 5 && r->pendingDrops == 0;
    caseResult("F5", ok, d.err);
    freePsram(r);
  }

  // F6 ---------------------------------------------------------------------
  {
    resetDoubles(); g_free = 60000;
    DiagRecorder r;
    bool ok = g_heapAllocs == 0 && r.mem == nullptr && !r.active;
    ok = ok && r.start(hp) == DIAG_START_OK && g_heapAllocs == 1 && g_psramAllocs == 0 && g_lastAllocSize >= DIAG_REC_BLOCK_SIZE && g_lastAllocSize < DIAG_REC_BLOCK_SIZE + 1024u;
    r.recPicCommand("X=1", 3);
    ok = ok && r.nBlocks == 1 && r.hasData();
    ok = ok && r.start(hp) == DIAG_START_ALREADY && g_heapAllocs == 1;
    ok = ok && r.stop() && g_heapFrees == 1 && r.mem == nullptr && r.ring == nullptr && !r.active;
    r.recPicCommand("X=2", 3);                              // after stop: a no-op
    ok = ok && g_heapAllocs == 1 && g_heapFrees == 1;
    ok = ok && r.start(hp) == DIAG_START_OK && g_heapAllocs == 2;   // a new recording allocates again
    r.stop();
    ok = ok && g_heapFrees == 2;
    caseResult("F6", ok, "allocs=" + std::to_string(g_heapAllocs) + " frees=" + std::to_string(g_heapFrees));
  }

  // F7 ---------------------------------------------------------------------
  {
    resetDoubles();
    const uint32_t total = DIAG_REC_BLOCK_SIZE + (1u << DIAG_REC_HEAP_OT_BITS) * 6u + (1u << DIAG_REC_HEAP_MQ_BITS) * 6u;
    DiagRecorder a; g_free = total + DIAG_REC_MIN_FREE_AFTER - 1;
    DiagStartResult ra = a.start(hp);
    bool ok = ra == DIAG_REFUSED_HEAP && g_heapAllocs == 0 && !a.active && std::strlen(diagStartReasonText(ra)) > 0 && a.lastStart == DIAG_REFUSED_HEAP;
    std::string why = "5119 left: result " + std::to_string((int)ra) + " allocs " + std::to_string(g_heapAllocs);
    DiagRecorder b; g_free = total + DIAG_REC_MIN_FREE_AFTER;
    ok = ok && b.start(hp) == DIAG_START_OK && g_heapAllocs == 1; b.stop();
    DiagRecorder c; g_free = 60000; g_failAlloc = true;
    DiagStartResult rc = c.start(hp);
    ok = ok && rc == DIAG_REFUSED_ALLOC && !c.active;
    DiagRecorder d; g_free = 3000; g_failAlloc = false; g_heapAllocs = 0;
    ok = ok && d.start(hp) == DIAG_REFUSED_HEAP && g_heapAllocs == 0;   // far below
    caseResult("F7", ok, why);
  }

  // F8 ---------------------------------------------------------------------
  {
    resetDoubles();
    DiagRecorder r;
    bool ok = r.start(ps) == DIAG_START_OK && r.nBlocks == DIAG_REC_PSRAM_BLOCKS && r.ringBytes() == 1536u * 1024u;
    r.recPicCommand("A=1", 3);
    ok = ok && r.stop() && r.start(ps) == DIAG_START_OK;
    ok = ok && g_psramAllocs == 1 && g_heapAllocs == 0 && g_heapFrees == 0 && r.mem != nullptr;
    r.recPicCommand("A=2", 3);
    Dec d = decodeBlock(r.block(0));
    ok = ok && d.recs.size() == 1 && d.recs[0] == "PIC A=2";    // a restart is a new recording
    r.stop();
    std::free(r.mem);
    DiagRecorder q; g_psramAvail = false;
    ok = ok && q.start(ps) == DIAG_REFUSED_ALLOC && !q.active;
    caseResult("F8", ok, "psramAllocs=" + std::to_string(g_psramAllocs));
  }

  // F9 ---------------------------------------------------------------------
  {
    resetDoubles(); g_free = 60000;
    DiagRecorder r;
    bool ok = r.resume(hp, 12, "Brownout", "panic in loop") == DIAG_START_OK;
    r.recPicCommand("PR=A", 4);
    Dec d = decodeBlock(r.block(0));
    ok = ok && d.recs.size() == 2 && d.recs[0] == "REBOOT 12 Brownout | panic in loop" && d.recs[1] == "PIC PR=A" && d.firstRec == 0;
    r.stop();
    DiagRecorder t; g_free = 5000; g_heapAllocs = 0;
    ok = ok && t.resume(hp, 13, "Brownout", "") == DIAG_REFUSED_HEAP && !t.active && g_heapAllocs == 0 && !t.hasData();
    caseResult("F9", ok);
  }

  // F10 --------------------------------------------------------------------
  {
    char o[64]; bool ok = true; std::string why;
    auto chk = [&](bool c, const char* what) { if (!c) { ok = false; why += what; why += "; "; } };
    diagRedactSetting("httppasswd", "hunter2", o, sizeof o); chk(!std::strcmp(o, "set"), "httppasswd set");
    diagRedactSetting("MQTTpasswd", "", o, sizeof o); chk(!std::strcmp(o, "empty"), "mqtt empty");
    diagRedactSetting("MQTTuser", "bob", o, sizeof o); chk(!std::strcmp(o, "set"), "mqtt user");
    diagRedactSetting("SATblebindkey2", "00112233445566778899aabbccddeeff", o, sizeof o); chk(!std::strcmp(o, "set"), "bindkey");
    diagRedactSetting("SATblemac", "AA:BB:CC:DD:EE:FF", o, sizeof o); chk(!std::strcmp(o, "set"), "ble mac");
    diagRedactSetting("WebhookURLon", "http://x/hook?token=abc", o, sizeof o); chk(!std::strcmp(o, "set"), "webhook url");
    diagRedactSetting("SATweatherapikey", "k", o, sizeof o); chk(!std::strcmp(o, "set"), "api key");
    diagRedactSetting("SATweatherlat", "52.3702", o, sizeof o); chk(!std::strcmp(o, "52.4"), "lat 52.3702");
    diagRedactSetting("SATweatherlon", "-4.8951", o, sizeof o); chk(!std::strcmp(o, "-4.9"), "lon -4.8951");
    diagRedactSetting("SATweatherlat", "52.349", o, sizeof o); chk(!std::strcmp(o, "52.3"), "lat 52.349");
    diagRedactSetting("SATweatherlat", "-0.04", o, sizeof o); chk(!std::strcmp(o, "0.0"), "lat -0.04");
    diagRedactSetting("SATweatherlon", "abc", o, sizeof o); chk(!std::strcmp(o, "set"), "lon not a number");
    diagRedactSetting("SATtargettemp", "20.5", o, sizeof o); chk(!std::strcmp(o, "20.5"), "plain setting kept");
    diagRedactSetting("hostname", "OTGW", o, sizeof o); chk(!std::strcmp(o, "OTGW"), "hostname kept");
    char b1[] = "{\"mqttpasswd\":\"hunter2\",\"x\":1}"; diagRedactBody(b1, std::strlen(b1)); chk(!std::strstr(b1, "hunter2") && std::strstr(b1, "\"x\":1"), "json body");
    char b2[] = "user=bob&password=hunter2&mode=1"; diagRedactBody(b2, std::strlen(b2)); chk(!std::strstr(b2, "hunter2") && !std::strstr(b2, "bob") && std::strstr(b2, "mode=1"), "form body");
    char b3[] = "TT=21.5"; diagRedactBody(b3, std::strlen(b3)); chk(!std::strcmp(b3, "TT=21.5"), "command kept");
    // end to end: a setting and a REST body never reach the ring in clear
    resetDoubles(); DiagRecorder* r = newPsramRecorder(ps);
    r->recSetting("httppasswd", "hunter2"); r->recSetting("SATweatherlat", "52.3702"); r->recRestIn(3, 1, "{\"mqttpasswd\":\"hunter2\"}", 24);
    r->recMqttIn("set/x", "{\"token\":\"abcdef\"}", 18);
    std::string raw((const char*)r->block(0), DIAG_REC_BLOCK_SIZE);
    chk(raw.find("hunter2") == std::string::npos && raw.find("abcdef") == std::string::npos && raw.find("52.3702") == std::string::npos, "ring holds no secret");
    chk(raw.find("52.4") != std::string::npos, "rounded lat in ring");
    freePsram(r);
    caseResult("F10", ok, why);
  }

  // F11 --------------------------------------------------------------------
  {
    resetDoubles(); DiagRecorder* r = newPsramRecorder(ps);
    std::string big(300, 'x'), ok254(254, 'y');
    r->recMqttPublish("t/long", big.c_str()); r->recMqttPublish("t/whole", ok254.c_str());
    Dec d = decodeBlock(r->block(0));
    bool ok = d.err.empty() && d.recs.size() == 2 && d.recs[0] == "MQ " + hex(diagNameId("t/long"), 4) + " " + std::string(254, 'x') + "~" &&
              d.recs[1] == "MQ " + hex(diagNameId("t/whole"), 4) + " " + ok254;
    caseResult("F11", ok, d.err);
    freePsram(r);
  }

  // F12 --------------------------------------------------------------------
  {
    resetDoubles(); DiagRecorder r;
    r.env = &ps;
    r.recOtFrame('T', 1); r.recPicCommand("A=1", 3); r.recMqttPublish("a", "b"); r.recSetting("x", "y"); r.recRestIn(1, 1, "z", 1);
    r.recMqttIn("a", "b", 1); r.recWeather(1, 2, 3); r.recBle(0, 1, 2, 3, 4); r.recReboot(1, "a", "b"); r.recTimeSync(5); r.recResponse("a", 1); r.recOtdCommand("a", 1);
    caseResult("F12", g_lockTries == 0 && !r.hasData() && r.recSeq == 0, "lockTries=" + std::to_string(g_lockTries));
  }

  // F13 --------------------------------------------------------------------
  {
    resetDoubles(); DiagRecorder* r = newPsramRecorder(ps);
    uint32_t t = g_ms; std::vector<uint32_t> at;
    const uint32_t gaps[] = { 1, 127, 128, 16383, 16384, 3600000 };
    for (uint32_t g : gaps) { g_ms += g; r->recPicCommand("A=1", 3); at.push_back(g_ms); }
    Dec d = decodeBlock(r->block(0));
    bool ok = d.err.empty() && d.times == at && d.uptime == t + 1u;
    caseResult("F13", ok, d.err);
    freePsram(r);
  }

  // F14 --------------------------------------------------------------------
  {
    bool ok = diagX100(21.456f) == 2146 && diagX100(-0.004f) == 0 && diagX100(-21.456f) == -2146 &&
              diagX100(1.0e9f) == 32767 && diagX100(-1.0e9f) == -32767 && diagX100((float)NAN) == INT16_MIN;
    caseResult("F14", ok);
  }

  // F15 --------------------------------------------------------------------
  {
    resetDoubles(); DiagRecorder* r = newPsramRecorder(ps);
    r->bridgeLine = true;                                    // a command line that came from the port 25238 bridge
    r->recOtdCommand("TT=21", 5); r->recPicCommand("TT=21", 5);
    r->bridgeLine = false;
    r->recOtdCommand("SB=1", 4);                             // a command from elsewhere is still recorded
    r->bridgeLine = true; r->recMqttPublish("a", "b"); r->bridgeLine = false;   // the flag covers commands only
    Dec d = decodeBlock(r->block(0));
    bool ok = d.err.empty() && d.recs.size() == 2 && d.recs[0] == "OTD SB=1" && d.recs[1] == "MQ " + hex(diagNameId("a"), 4) + " b";
    caseResult("F15", ok, d.err + " n=" + std::to_string(d.recs.size()));
    freePsram(r);
  }

  std::printf("%s\n", g_fail ? "SOME CASES FAILED" : "ALL CASES PASSED");
  return g_fail ? 1 : 0;
}
