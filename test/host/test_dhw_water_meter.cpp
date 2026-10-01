// test/host/test_dhw_water_meter.cpp  (TASK-1123, ADR-176)
//
// The cumulative DHW water total through the REAL code. test_dhw_water_meter.py
// slices, by anchor:
//   - dhwWaterMeter.ino (always the working tree: the file is new in TASK-1123):
//     the accumulator, the write-rate rule, save / flush / load, and the reset,
//     whose cases are in test_dhw_water_discovery.cpp;
//   - OTGW-Core.ino, from the revision under test: print_f88() and
//     updatePSSummaryFloatState() (the two MsgID 19 write sites ADR-176 names), the
//     validity gates print_f88() calls, the f8.8 codec, the spec-profile helpers and
//     parseStrictFloat(), which turns a PS=1 summary field into the float that
//     updatePSSummaryFloatState() receives;
//   - OTGW-Core.h, Hardwaretypes.h, OTGW-firmware.h: the frame, lookup, state and
//     hardware-mode types, isOTDirectEnabled() and isFlashing().
// This file supplies the platform (dhw_host_shim.h), the log / MQTT / override-store
// doubles and the cases. Frames are fed to print_f88() the way processOT() does it:
// OTdata filled in, then decodeAndPublishOTValue()'s case OT_DHWFlowRate call. The
// A-frame flags are set as processOT()'s pairing would set them (OTGW-Core.ino, the
// bGatewaySubstituted / bAnswerOverride block): that pairing is not itself run here.

#include "dhw_host_shim.h"

#define HAS_DIRECT_OT 1

// ---- types from OTGW-Core.h and Hardwaretypes.h, sliced ------------------------------
#include "gen_types.inc"

// ---- firmware state the slices touch ---------------------------------------------------
struct {
  struct { uint32_t iTrThermostatMs = 0; } otBus;
  struct { OTGWHardwareMode eMode = HW_MODE_PIC; } hw;
  struct { bool bESPactive = false; bool bPICactive = false; } flash;
} state;
static OTdataStruct OTcurrentSystemState;   // OTGW-Core.h declares it the same way
OpenthermData_t OTdata;
OTlookup_t      OTlookupitem;

// ---- I/O doubles for print_f88() -------------------------------------------------------
static void AddLogf(const char*, ...) {}
static int  g_overrideCaptures = 0;
void recordOTOverride(uint8_t, uint8_t, float) { g_overrideCaptures++; }
static const char* messageIDToString(OTLibMessageID) { return "DHWFlowRate"; }
static bool sendMQTTData(const char*, const char*, const bool = false) { return true; }
static void publishToSourceTopic(const char*, const char*, byte) {}
// The reset in dhwWaterMeter.ino publishes through sendDHWWaterTotal() (MQTTstuff.ino).
// test_dhw_water_discovery.cpp runs the real one; here it only counts.
static int  g_totalSends = 0;
static void sendDHWWaterTotal() { g_totalSends++; }

// ---- isOTDirectEnabled(), isFlashing() from OTGW-firmware.h, sliced -------------------
#include "gen_fw_h.inc"
// ---- dhwWaterMeter.ino, sliced ----------------------------------------------------------
#include "gen_meter.inc"
// ---- OTGW-Core.ino, sliced (revision under test) ---------------------------------------
#include "gen_core.inc"

// ---- harness ----------------------------------------------------------------------------
static const char* kFile = "/dhw_water.json";

// A reboot: the module's RAM state back to its initial values. The statics are sliced
// into this translation unit, so the harness can reach them.
static void rebootMeterRam() {
  dhwWaterTotalL = 0.0; dhwMeterLastMs = 0; dhwMeterSeeded = false;
  dhwMeterSavedL = 0.0; dhwMeterSavedMs = 0; dhwMeterResetPending = false;
}

static void freshBoot(OTGWHardwareMode mode) {
  rebootMeterRam();
  OTcurrentSystemState = OTdataStruct();
  state.hw.eMode = mode;
  state.flash.bESPactive = state.flash.bPICactive = false;
  LittleFS.files.clear(); LittleFS.writes = 0; LittleFS.failedCommits = 0; LittleFS.failNextCommit = false;
  LittleFSmounted = true;
  g_ms = 5000;
}

static void at(uint32_t ms) { g_ms = ms; }

// One MsgID 19 Read-Ack reaching print_f88(). src 'B' = boiler, 'A' = gateway answer.
static void frame19(char src, float flowLpm, bool answerOverride = false, bool gatewaySubstituted = false) {
  OpenthermData_t d{};
  d.type = OT_READ_ACK; d.masterslave = 1; d.id = 19;
  d.f88(flowLpm);
  d.rsptype = (src == 'B') ? OTGW_BOILER : OTGW_ANSWER_THERMOSTAT;
  d.bAnswerOverride = answerOverride ? 1 : 0;
  d.bGatewaySubstituted = gatewaySubstituted ? 1 : 0;
  d.time = (time_t)g_ms;
  OTdata = d;
  OTlookupitem = OTlookup_t{19, OT_READ, ot_f88, "DHWFlowRate", "DHW flow rate", "l/min", true};
  print_f88(OTcurrentSystemState.DHWFlowRate);   // decodeAndPublishOTValue(): case OT_DHWFlowRate
}

// One PS=1 summary field for MsgID 19 reaching its state write site.
static void summary19(float flowLpm) { updatePSSummaryFloatState(19, flowLpm); }

static bool near(double a, double b, double tol = 1e-6) { return std::fabs(a - b) <= tol; }
static double fileLitres() {
  auto it = LittleFS.files.find(kFile);
  if (it == LittleFS.files.end()) return -1.0;
  const char* p = std::strstr(it->second.c_str(), "\"litres\":");
  return p ? std::atof(p + 9) : -1.0;
}

static char g_got[512];

// ---- W: through the write sites ----------------------------------------------------------
static void caseW1() {
  freshBoot(HW_MODE_PIC);
  for (uint32_t s = 0; s <= 60; s += 10) { at(5000 + s * 1000); frame19('B', 6.0f); }
  std::snprintf(g_got, sizeof g_got, "total=%.4f L, DHWFlowRate state=%.2f", dhwWaterTotalL, OTcurrentSystemState.DHWFlowRate);
  verdict("W1", "boiler B frames through print_f88(), 6 L/min every 10 s for 60 s: 6.0 L",
          near(dhwWaterTotalL, 6.0) && near(OTcurrentSystemState.DHWFlowRate, 6.0), g_got);
}

static void caseW2() {
  // ADR-181: the PS=1 summary updates the flow-rate state but never the total.
  freshBoot(HW_MODE_PIC);
  for (uint32_t s = 0; s <= 60; s += 10) { at(5000 + s * 1000); summary19(6.0f); }
  std::snprintf(g_got, sizeof g_got, "total=%.4f L, DHWFlowRate state=%.2f", dhwWaterTotalL, OTcurrentSystemState.DHWFlowRate);
  verdict("W2", "PIC gateway, PS=1 summaries through updatePSSummaryFloatState(), 6 L/min every 10 s for 60 s: state 6.0, total 0 L",
          near(dhwWaterTotalL, 0.0) && near(OTcurrentSystemState.DHWFlowRate, 6.0), g_got);
}

static void caseW3() {
  // OT-Direct master mode: the scheduler's B reply seeds the meter at 8 L/min, then the
  // boiler stops answering. The thermostat asks MsgID 19 every second and
  // handleMasterModeSlaveFrame() answers from otBoilerCache: a T then an A, no B between,
  // so processOT() leaves bAnswerOverride = 0 (a proxy A, master-valid per ADR-103).
  freshBoot(HW_MODE_OT_DIRECT);
  at(5000); frame19('B', 8.0f);
  OTcurrentSystemState.DHWFlowRate = 0.0f;          // so the A frames must write it back
  for (uint32_t s = 1; s <= 120; s++) { at(5000 + s * 1000); frame19('A', 8.0f); }
  std::snprintf(g_got, sizeof g_got, "total=%.4f L, DHWFlowRate state=%.2f (written by the A frames)",
                dhwWaterTotalL, OTcurrentSystemState.DHWFlowRate);
  verdict("W3", "OT-Direct master mode, cache replay as proxy A every 1 s for 120 s: reaches state, adds 0 L",
          near(dhwWaterTotalL, 0.0) && near(OTcurrentSystemState.DHWFlowRate, 8.0), g_got);
}

static void caseW4() {
  // A PIC answer override (B followed within 500 ms by A) carries bAnswerOverride = 1 on
  // the A and bGatewaySubstituted = 1 on the B. The A is not master-valid, so the validity
  // gate stops it before the source filter matters: removing that filter (mutant M2) does
  // not change this case. The B frames still count.
  freshBoot(HW_MODE_PIC);
  for (uint32_t s = 0; s <= 60; s += 10) {
    at(5000 + s * 1000);       frame19('B', 6.0f, false, true);
    at(5000 + s * 1000 + 200); frame19('A', 5.0f, true, false);
  }
  std::snprintf(g_got, sizeof g_got, "total=%.4f L, DHWFlowRate state=%.2f, override captures=%d",
                dhwWaterTotalL, OTcurrentSystemState.DHWFlowRate, g_overrideCaptures);
  verdict("W4", "PIC answer override A after each B: the validity gate keeps the A out; total and state follow the B frames (6.0 L)",
          near(dhwWaterTotalL, 6.0) && near(OTcurrentSystemState.DHWFlowRate, 6.0), g_got);
}

static void caseW5() {
  // OT-Direct with PS=1: bridgeFrameToParser() still feeds every frame to processOT(), and
  // emitSummaryLine() echoes OTcurrentSystemState after each MsgID 0 reply (every 800 ms).
  // B frames for MsgID 19 every 10 s for 60 s, then the boiler stops answering MsgID 19
  // while the echo goes on for five more minutes with the last value.
  freshBoot(HW_MODE_OT_DIRECT);
  uint32_t nextB = 0, nextEcho = 800;
  const uint32_t endMs = 360000;
  while (nextB <= 60000 || nextEcho <= endMs) {
    if (nextB <= 60000 && nextB <= nextEcho) { at(5000 + nextB); frame19('B', 6.0f); nextB += 10000; }
    else { at(5000 + nextEcho); summary19(OTcurrentSystemState.DHWFlowRate); nextEcho += 800; }
  }
  std::snprintf(g_got, sizeof g_got, "total=%.4f L after 60 s of B frames and 360 s of summary echo", dhwWaterTotalL);
  verdict("W5", "OT-Direct PS=1: only the B frames count (6.0 L); the echo neither doubles them nor keeps counting after they stop",
          near(dhwWaterTotalL, 6.0), g_got);
}

static void caseW6() {
  // A PIC gateway in PS=1 mode with 5000 L restored. Each MsgID 19 field goes through the
  // real parseStrictFloat() and updatePSSummaryFloatState(), 10 s apart;
  // publishPSSummaryFieldValue(), which runs between the two in the firmware, is not run
  // here. strtod() accepts "nan", "inf" and exponents, so malformed fields parse. Under
  // ADR-181 no summary field reaches the meter, valid or not, so the restored total must
  // come through unchanged. The values below fit that function's 12-byte print buffer;
  // the accumulator's own bound on such readings is case U7. Then an orderly restart and
  // a boot restore the file.
  freshBoot(HW_MODE_PIC);
  LittleFS.files[kFile] = "{\"litres\":5000.000}";
  loadDHWWaterMeter();
  static const char* fields[] = { "6.50", "6.50", "nan", "inf", "999.5", "-inf", "6.50" };
  int parsed = 0;
  uint32_t t = 5000;
  for (const char* field : fields) {
    float v = 0.0f;
    if (parseStrictFloat(field, v)) { parsed++; at(t); updatePSSummaryFloatState(19, v); }
    t += 10000;
  }
  const double total = dhwWaterTotalL;
  flushDHWWaterMeter();                            // doRestart()
  const std::string file = LittleFS.files[kFile];
  rebootMeterRam();
  loadDHWWaterMeter();                             // setup() on the next boot
  const double expected = 5000.0;                  // nothing from the summary counts (ADR-181)
  std::snprintf(g_got, sizeof g_got, "fields parsed=%d of 7, total=%.4f L (want %.4f), file %s, after reboot %.4f L",
                parsed, total, expected, file.c_str(), dhwWaterTotalL);
  verdict("W6", "PIC PS=1 fields 6.50, nan, inf, 999.5 and -inf through the real parser add nothing; the total, file and reboot stay at 5000 L",
          parsed == 7 && near(total, expected, 1e-4) && near(dhwWaterTotalL, expected, 0.0005), g_got);
}

static void caseR1() {
  // PIC in PS=1 mode. The PIC reports its stored MsgID 19 Read-Ack on every PS=1 request
  // (gateway.asm HandleResponse -> StoreValue, SummaryReport -> PrintStoredVal). If the
  // thermostat stopped requesting MsgID 19 while that stored value was 8 L/min, a client
  // polling PS=1 every 30 s keeps presenting 8 L/min, and each interval is under the cap.
  // Counted, that added 80 L in 10 minutes; ADR-181 keeps the summary out of the total.
  freshBoot(HW_MODE_PIC);
  for (uint32_t s = 0; s <= 600; s += 30) { at(5000 + s * 1000); summary19(8.0f); }
  std::snprintf(g_got, sizeof g_got, "total=%.4f L after 10 min of a repeated stored 8 L/min", dhwWaterTotalL);
  verdict("R1", "a PIC summary that repeats a stale stored value adds nothing (0 L; 80 L when the summary counted)",
          near(dhwWaterTotalL, 0.0), g_got);
}

// ---- U: the accumulator itself ---------------------------------------------------------------
static void caseU1() {
  freshBoot(HW_MODE_PIC);
  for (uint32_t s = 0; s <= 30; s += 10) { at(s * 1000); updateDHWWaterMeter(6.0f, g_ms); }
  const double coarse = dhwWaterTotalL;
  freshBoot(HW_MODE_PIC);
  for (uint32_t s = 0; s <= 30; s += 5) { at(s * 1000); updateDHWWaterMeter(6.0f, g_ms); }
  const double fine = dhwWaterTotalL;
  std::snprintf(g_got, sizeof g_got, "every 10 s: %.4f L, every 5 s: %.4f L", coarse, fine);
  verdict("U1", "time-based: 6 L/min for 30 s is 3.0 L whether sampled every 10 s or every 5 s",
          near(coarse, 3.0) && near(fine, 3.0), g_got);
}

static void caseU2() {
  freshBoot(HW_MODE_PIC);
  at(0);       updateDHWWaterMeter(6.0f, g_ms);   // seed
  at(60000);   updateDHWWaterMeter(6.0f, g_ms);   // dt = 60000, at the cap: counts 6.0 L
  const double atCap = dhwWaterTotalL;
  at(120001);  updateDHWWaterMeter(6.0f, g_ms);   // dt = 60001: a gap, counts nothing
  const double afterGap = dhwWaterTotalL;
  at(130001);  updateDHWWaterMeter(6.0f, g_ms);   // dt = 10000 after the gap: 1.0 L
  std::snprintf(g_got, sizeof g_got, "after 60000 ms: %.4f L, after a 60001 ms gap: %.4f L, 10 s later: %.4f L",
                atCap, afterGap, dhwWaterTotalL);
  verdict("U2", "gap cap: 60000 ms counts, 60001 ms counts nothing, the next interval counts again (7.0 L)",
          near(atCap, 6.0) && near(afterGap, 6.0) && near(dhwWaterTotalL, 7.0), g_got);
}

static void caseU3() {
  freshBoot(HW_MODE_PIC);
  at(0);     updateDHWWaterMeter(6.0f, g_ms);
  at(10000); updateDHWWaterMeter(6.0f, g_ms);
  updateDHWWaterMeter(6.0f, g_ms);                // the same exchange presented again...
  updateDHWWaterMeter(6.0f, g_ms);                // ...and again, at the same millis()
  std::snprintf(g_got, sizeof g_got, "total=%.4f L", dhwWaterTotalL);
  verdict("U3", "duplicate samples of one exchange add nothing: 1.0 L, not 3.0 L",
          near(dhwWaterTotalL, 1.0), g_got);
}

static void caseU4() {
  freshBoot(HW_MODE_PIC);
  for (uint32_t s = 0; s <= 600; s += 10) { at(s * 1000); updateDHWWaterMeter(0.0f, g_ms); }
  const double idle = dhwWaterTotalL;
  at(610000); updateDHWWaterMeter(6.0f, g_ms);
  std::snprintf(g_got, sizeof g_got, "10 min at 0 L/min: %.4f L, then one 6 L/min sample 10 s later: %.4f L", idle, dhwWaterTotalL);
  verdict("U4", "zero flow adds nothing and keeps the clock; the first non-zero sample counts its own interval (1.0 L)",
          near(idle, 0.0) && near(dhwWaterTotalL, 1.0), g_got);
}

static void caseU5() {
  freshBoot(HW_MODE_PIC);
  at(0xFFFFF000u); updateDHWWaterMeter(6.0f, g_ms);
  at(0x00000FA0u); updateDHWWaterMeter(6.0f, g_ms);   // 8096 ms later, across the wrap
  std::snprintf(g_got, sizeof g_got, "total=%.6f L", dhwWaterTotalL);
  verdict("U5", "millis() wrap: 8096 ms across 2^32 counts 0.8096 L", near(dhwWaterTotalL, 0.8096), g_got);
}

static void caseU6() {
  freshBoot(HW_MODE_PIC);
  const bool before = dhwWaterMeterHasData();
  at(0);     updateDHWWaterMeter(6.0f, g_ms);
  const bool afterSeed = dhwWaterMeterHasData();
  const double seedTotal = dhwWaterTotalL;
  at(10000); updateDHWWaterMeter(-3.0f, g_ms);
  std::snprintf(g_got, sizeof g_got, "hasData before=%d after first sample=%d, first sample adds %.4f L, negative flow total %.4f L",
                (int)before, (int)afterSeed, seedTotal, dhwWaterTotalL);
  verdict("U6", "the first sample only seeds (0 L, hasData true); a negative flow adds nothing",
          !before && afterSeed && near(seedTotal, 0.0) && near(dhwWaterTotalL, 0.0), g_got);
}

static void caseU7() {
  // Readings an f8.8 frame cannot carry, each 10 s after the previous sample: NaN, +inf,
  // -inf, 1e30 and 128.5 L/min. None adds water, and each still moves the clock, so the
  // next valid sample counts only its own 10 s. 127.99 L/min, the top of the f8.8 range,
  // counts.
  freshBoot(HW_MODE_PIC);
  static const float bad[] = { NAN, INFINITY, -INFINITY, 1e30f, 128.5f };
  uint32_t t = 0;
  at(t); updateDHWWaterMeter(6.0f, g_ms);          // seed
  std::string totals;
  for (float v : bad) {
    t += 10000; at(t); updateDHWWaterMeter(v, g_ms);
    char one[32]; std::snprintf(one, sizeof one, "%s%.4f", totals.empty() ? "" : " ", dhwWaterTotalL);
    totals += one;
  }
  const double afterBad = dhwWaterTotalL;
  t += 10000; at(t); updateDHWWaterMeter(127.99f, g_ms);
  const double expected = (double)127.99f * 10000.0 / 60000.0;
  std::snprintf(g_got, sizeof g_got, "total after each of NaN, +inf, -inf, 1e30, 128.5: %s L; then 127.99 L/min for 10 s: %.4f L (want %.4f)",
                totals.c_str(), dhwWaterTotalL, expected);
  verdict("U7", "a flow f8.8 cannot carry (NaN, infinite, above 128 L/min) adds nothing and keeps the clock; 127.99 L/min counts",
          afterBad == 0.0 && std::isfinite(dhwWaterTotalL) && near(dhwWaterTotalL, expected, 1e-6), g_got);
}

// ---- P: persistence ------------------------------------------------------------------------------
static float flowAtSecond(uint32_t s) {
  struct Draw { uint32_t from, to; float lpm; };
  static const Draw draws[] = {
    { 7*3600,        7*3600 + 480,  8.0f},   // shower, 64 L
    { 7*3600 + 1800, 7*3600 + 1830, 6.0f},   // tap, 3 L
    {12*3600,       12*3600 + 60,   6.0f},   // tap, 6 L
    {18*3600,       18*3600 + 180,  7.0f},   // dishes, 21 L
    {21*3600,       21*3600 + 600, 12.0f},   // bath, 120 L
    {22*3600,       22*3600 + 20,   6.0f},   // tap, 2 L
  };
  for (const Draw& d : draws) if (s >= d.from && s < d.to) return d.lpm;
  return 0.0f;
}

static void caseP1() {
  // 24 h plus 20 min: a MsgID 19 sample every 10 s, the 60 s task every minute.
  freshBoot(HW_MODE_PIC);
  const uint32_t kInterval = DHW_METER_SAVE_INTERVAL_MS;
  int ticks = 0, idleTicksWritten = 0, earlyWrites = 0, missedWrites = 0, multiWrites = 0, lagAfterTick = 0;
  for (uint32_t s = 0; s <= 24 * 3600 + 1200; s += 10) {
    at(s * 1000);
    updateDHWWaterMeter(flowAtSecond(s), g_ms);
    if (s % 60 != 0) continue;
    ticks++;
    const double   unsaved = dhwWaterTotalL - dhwMeterSavedL;
    const uint32_t since   = g_ms - dhwMeterSavedMs;
    const int      before  = LittleFS.writes;
    saveDHWWaterMeterIfDue(g_ms);                            // doTaskEvery60s()
    const int wrote = LittleFS.writes - before;
    const bool due = unsaved >= 10.0 || (unsaved > 0.0 && since >= kInterval);
    if (wrote > 1) multiWrites++;
    if (unsaved <= 0.0 && wrote) idleTicksWritten++;
    if (wrote && !due) earlyWrites++;
    if (!wrote && due) missedWrites++;
    if (dhwWaterTotalL - dhwMeterSavedL >= 10.0) lagAfterTick++;
  }
  std::snprintf(g_got, sizeof g_got,
                "total=%.3f L file=%.3f L writes=%d over %d ticks; idle-tick writes=%d early=%d missed=%d multi=%d lag>=10L=%d",
                dhwWaterTotalL, fileLitres(), LittleFS.writes, ticks, idleTicksWritten, earlyWrites, missedWrites,
                multiWrites, lagAfterTick);
  verdict("P1", "24 h, 216 L drawn: writes only when >= 10 L unsaved or 15 min with some unsaved, never idle, at most 1 per tick",
          near(dhwWaterTotalL, 216.0) && near(fileLitres(), 216.0, 0.0005) && idleTicksWritten == 0 &&
          earlyWrites == 0 && missedWrites == 0 && multiWrites == 0 && lagAfterTick == 0 && LittleFS.writes > 0,
          g_got);
}

static void caseP2() {
  // The file is present, so only the unsaved branch of the restart flush can write it.
  freshBoot(HW_MODE_PIC);
  LittleFS.files[kFile] = "{\"litres\":50.000}";
  loadDHWWaterMeter();                             // setup(): 50 L restored and saved
  at(0);     updateDHWWaterMeter(6.0f, g_ms);
  at(32000); updateDHWWaterMeter(6.0f, g_ms);      // 3.2 L, under both thresholds
  saveDHWWaterMeterIfDue(g_ms);
  const int afterTick = LittleFS.writes;
  flushDHWWaterMeter();                            // doRestart()
  const int afterFlush = LittleFS.writes;
  const double persisted = fileLitres();
  flushDHWWaterMeter();                            // nothing new
  std::snprintf(g_got, sizeof g_got, "file 50 L, 3.2 L unsaved: tick writes=%d, flush writes=%d (file %.3f L), second flush writes=%d",
                afterTick, afterFlush - afterTick, persisted, LittleFS.writes - afterFlush);
  verdict("P2", "orderly restart with the file present: 3.2 L the rule holds back are flushed once (53.2 L); a second flush writes nothing",
          afterTick == 0 && afterFlush == 1 && near(persisted, 53.2, 0.0005) && LittleFS.writes == 1, g_got);
}

static void caseP3() {
  freshBoot(HW_MODE_PIC);
  for (uint32_t s = 0; s <= 500; s += 10) { at(s * 1000); updateDHWWaterMeter(6.0f, g_ms); }   // 50 L
  flushDHWWaterMeter();
  const int w0 = LittleFS.writes;
  LittleFS.files.clear();                          // a filesystem OTA swaps in an image without the file
  flushDHWWaterMeter();
  const int restored = LittleFS.writes - w0;
  const double persisted = fileLitres();
  freshBoot(HW_MODE_PIC);                          // total 0 and no file: nothing to keep
  flushDHWWaterMeter();
  std::snprintf(g_got, sizeof g_got, "file gone with 50 L saved: flush writes=%d (file %.3f L); total 0, no file: writes=%d",
                restored, persisted, LittleFS.writes);
  verdict("P3", "filesystem OTA: the restart flush rewrites a missing file even with nothing unsaved; not for a zero total",
          restored == 1 && near(persisted, 50.0, 0.0005) && LittleFS.writes == 0, g_got);
}

static void caseP4() {
  freshBoot(HW_MODE_PIC);
  dhwWaterTotalL = 12345.678;
  flushDHWWaterMeter();
  rebootMeterRam();
  at(123456);
  loadDHWWaterMeter();                             // setup()
  const bool dueNow = dhwWaterMeterSaveDue(g_ms);
  std::snprintf(g_got, sizeof g_got, "restored %.3f L, hasData=%d, write due right after boot=%d",
                dhwWaterTotalL, (int)dhwWaterMeterHasData(), (int)dueNow);
  verdict("P4", "reboot round-trip: 12345.678 L restored; a restored total alone is not data and needs no write",
          near(dhwWaterTotalL, 12345.678, 0.0005) && !dhwWaterMeterHasData() && !dueNow, g_got);
}

static void caseP5() {
  // 1e21 is the first total whose 1.x-style dtostrf rendering no longer fits msg[24] in
  // publishDHWWaterMeter(); 10000000000.5 is just above the loader's bound.
  static const char* bad[] = { "{\"litres\":-5}", "{\"litres\":nan}", "{\"litres\":inf}", "{\"foo\":1}", "{\"litres\":}", "",
                               "{\"litres\":1e21}", "{\"litres\":1e300}", "{\"litres\":10000000000.5}" };
  const int nBad = (int)(sizeof(bad) / sizeof(bad[0]));
  int accepted = 0;
  std::string which;
  for (const char* content : bad) {
    freshBoot(HW_MODE_PIC);
    LittleFS.files[kFile] = content;
    loadDHWWaterMeter();
    if (dhwWaterTotalL != 0.0) { accepted++; which += std::string(" ") + content; }
  }
  freshBoot(HW_MODE_PIC);
  loadDHWWaterMeter();                             // no file at all
  if (dhwWaterTotalL != 0.0) { accepted++; which += " (no file)"; }
  freshBoot(HW_MODE_PIC);
  LittleFS.files[kFile] = "{\"litres\":10000000000}";
  loadDHWWaterMeter();                             // exactly at the bound: a value, restored
  const double atBound = dhwWaterTotalL;
  std::snprintf(g_got, sizeof g_got, "bad or missing files accepted=%d of %d%s; 1e10 L restored as %.1f L",
                accepted, nBad + 1, which.c_str(), atBound);
  verdict("P5", "load: negative, NaN, infinite, keyless, empty, missing, 1e21, 1e300 or above 1e10 L leaves the total at 0; 1e10 L loads",
          accepted == 0 && atBound == 1e10, g_got);
}

static void caseP6() {
  freshBoot(HW_MODE_PIC);
  for (uint32_t s = 0; s <= 200; s += 10) { at(s * 1000); updateDHWWaterMeter(6.0f, g_ms); }   // 20 L
  state.flash.bESPactive = true;                   // an OTA is running
  saveDHWWaterMeterIfDue(g_ms); flushDHWWaterMeter();
  const int whileFlashing = LittleFS.writes;
  state.flash.bESPactive = false;
  LittleFSmounted = false;
  saveDHWWaterMeterIfDue(g_ms); flushDHWWaterMeter();
  const int whileUnmounted = LittleFS.writes - whileFlashing;
  LittleFSmounted = true;
  saveDHWWaterMeterIfDue(g_ms);
  std::snprintf(g_got, sizeof g_got, "writes while flashing=%d, while unmounted=%d, afterwards=%d",
                whileFlashing, whileUnmounted, LittleFS.writes);
  verdict("P6", "no LittleFS write while flashing or unmounted; the pending write happens afterwards",
          whileFlashing == 0 && whileUnmounted == 0 && LittleFS.writes == 1, g_got);
}

static void caseP7() {
  // A commit that fails without an error (a full filesystem): print() reported every
  // byte and close() said nothing, while the file kept the previous 50 L.
  // Phase 1: the restart flush comes before the next tick.
  freshBoot(HW_MODE_PIC);
  LittleFS.files[kFile] = "{\"litres\":50.000}";
  loadDHWWaterMeter();                             // 50 L restored and saved
  for (uint32_t s = 0; s <= 120; s += 10) { at(s * 1000); updateDHWWaterMeter(6.0f, g_ms); }   // + 12 L
  LittleFS.failNextCommit = true;
  saveDHWWaterMeterIfDue(g_ms);                    // due (12 L unsaved), the commit fails
  const double savedAfterFail = dhwMeterSavedL;
  const double fileAfterFail  = fileLitres();
  flushDHWWaterMeter();                            // doRestart()
  const double fileAfterFlush = fileLitres();
  const int failed1 = LittleFS.failedCommits, commits1 = LittleFS.writes;
  // Phase 2: the next tick comes first.
  freshBoot(HW_MODE_PIC);
  LittleFS.files[kFile] = "{\"litres\":50.000}";
  loadDHWWaterMeter();
  for (uint32_t s = 0; s <= 120; s += 10) { at(s * 1000); updateDHWWaterMeter(6.0f, g_ms); }
  LittleFS.failNextCommit = true;
  saveDHWWaterMeterIfDue(g_ms);
  at(g_ms + 60000);
  saveDHWWaterMeterIfDue(g_ms);                    // the next 60 s tick
  const double fileAfterRetry = fileLitres();
  const int failed2 = LittleFS.failedCommits, commits2 = LittleFS.writes;
  std::snprintf(g_got, sizeof g_got,
                "failed commit: saved mark=%.3f L file=%.3f L; restart flush -> file=%.3f L (failed=%d commits=%d) | "
                "next tick -> file=%.3f L (failed=%d commits=%d)",
                savedAfterFail, fileAfterFail, fileAfterFlush, failed1, commits1, fileAfterRetry, failed2, commits2);
  verdict("P7", "a silently failed commit keeps the saved mark at 50 L, so the restart flush or the next tick still writes 62 L",
          near(savedAfterFail, 50.0) && near(fileAfterFail, 50.0, 0.0005) && near(fileAfterFlush, 62.0, 0.0005) &&
          failed1 == 1 && commits1 == 1 && near(fileAfterRetry, 62.0, 0.0005) && failed2 == 1 && commits2 == 1, g_got);
}

int main() {
  std::printf("== DHW water total through the real code (TASK-1123, ADR-176) ==\n");
  caseW1(); caseW2(); caseW3(); caseW4(); caseW5(); caseW6(); caseR1();
  caseU1(); caseU2(); caseU3(); caseU4(); caseU5(); caseU6(); caseU7();
  caseP1(); caseP2(); caseP3(); caseP4(); caseP5(); caseP6(); caseP7();
  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
