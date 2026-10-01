// test/host/test_dhw_water_discovery.cpp  (TASK-1123, ADR-176)
//
// The dhw_water_total discovery entry, its state publish and the user reset, through
// the REAL code. test_dhw_water_meter.py slices, from the working tree:
//   - MQTTstuff.h: the Ha* enums, flag bits, row structs, the PROGMEM read helpers,
//     MQTT_HA_INDEX_NONE and HaDevice;
//   - MQTTHaDiscovery.cpp: every label/name string, the sensor and binary-sensor tables,
//     MQTT_HA_SENSOR_COUNT, both index tables and the enum-to-string functions;
//   - MQTTstuff.ino: the done / pending bitmaps, queueNonOTDiscoveryIds(),
//     publishNonOTDiscoveryConfigs(), markAllMQTTConfigPending(), deviceForOTId(),
//     readMQTTTopicToken(), sendDHWWaterTotal(), publishDHWWaterMeter(), and the
//     set/<node>/otgw/ branch of handleMQTTcallback();
//   - restAPI.ino: the reset_water_total branch of handleOtgw();
//   - OTGW-firmware.h: the faux discovery ids, the two bitmaps, the meter prototypes,
//     isFlashing();
//   - dhwWaterMeter.ino: the meter, its persistence and its reset.
// This file supplies the platform (dhw_host_shim.h), settings/state stubs, an MQTT
// double that records publishes, HTTP doubles that record the answer, and the cases.
// The two command branches run inside wrapper functions; their own 'return'
// statements end the wrapper, as they end the handler in the firmware.

#include "dhw_host_shim.h"

// ---- MQTTstuff.h types, sliced ---------------------------------------------------------
#include "gen_disc_types.inc"
// ---- MQTTHaDiscovery.cpp tables, sliced -------------------------------------------------
#include "gen_disc_table.inc"
// ---- MQTTstuff.h helpers and MQTTHaDiscovery.cpp enum-to-string functions, sliced -----
#include "gen_disc_helpers.inc"

// ---- firmware state the slices touch ---------------------------------------------------
struct {
  struct { uint32_t iPublishedTopicCount = 0; } discovery;
  struct { bool bESPactive = false; bool bPICactive = false; } flash;
} state;
struct {
  struct { bool bEnable = true; bool bLastPublishedLegacy = false; bool bLegacyMode = false; } mqtt;
} settings;

struct Pub { std::string topic, payload; bool retain; };
static std::vector<Pub> g_pubs;
static bool sendMQTTData(const char* t, const char* p, const bool r = false) { g_pubs.push_back({t, p, r}); return true; }
static bool sendMQTTData(const __FlashStringHelper* t, const char* p, const bool r = false) {
  return sendMQTTData(reinterpret_cast<const char*>(t), p, r);
}
static void armTopologyCleanup(bool) {}
void setMQTTConfigPending(const uint8_t MSGid);   // MQTTstuff.ino declares these ahead too
void markAllMQTTConfigPending();

// ---- HTTP doubles for the REST branch ----------------------------------------------------
enum HTTPMethod { HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE };
static int         g_httpStatus = 0;
static std::string g_httpBody;
static bool        g_cors = false;
static void sendApiMethodNotAllowed(const __FlashStringHelper*) { g_httpStatus = 405; }
static void sendCorsOriginHeader() { g_cors = true; }
static void webSend(int code, const __FlashStringHelper*, const __FlashStringHelper* body) {
  g_httpStatus = code; g_httpBody = reinterpret_cast<const char*>(body);
}

// ---- OTGW-firmware.h: faux ids, bitmaps, meter prototypes, isFlashing(), sliced ------
#include "gen_disc_fw_h.inc"
// ---- dhwWaterMeter.ino, sliced ------------------------------------------------------------
#include "gen_meter.inc"
// ---- MQTTstuff.ino, sliced ---------------------------------------------------------------
#include "gen_disc_mqtt.inc"

// ---- the two command branches, sliced ---------------------------------------------------
// restAPI.ino handleOtgw(): the body of the reset_water_total branch.
static void restResetWaterTotalBranch(HTTPMethod method) {
#include "gen_rest_reset_branch.inc"
}
// MQTTstuff.ino handleMQTTcallback(): the whole 'otgw' sub-token branch. topicToken is
// the command token after set/<node>/, topicCursor the rest of the topic behind it,
// retained the broker's retain flag of the message.
static void mqttOtgwBranch(const char* topicToken, const char* topicCursor, bool retained) {
#include "gen_mqtt_otgw_branch.inc"
}

// ---- harness ----------------------------------------------------------------------------
static const char* kFile = "/dhw_water.json";
static bool pending(uint8_t id) { return bitRead(MQTTautoCfgPendingMap[id >> 5], id & 0x1F) != 0; }
static std::string str(PGM_P p) { return p ? std::string(p) : std::string("(none)"); }
static double fileLitres() {
  auto it = LittleFS.files.find(kFile);
  if (it == LittleFS.files.end()) return -1.0;
  const char* p = std::strstr(it->second.c_str(), "\"litres\":");
  return p ? std::atof(p + 9) : -1.0;
}
static std::string lastPayload() { return g_pubs.empty() ? std::string("(none)") : g_pubs.back().payload; }
static bool near(double a, double b, double tol = 1e-6) { return std::fabs(a - b) <= tol; }

static void freshBoot() {
  dhwWaterTotalL = 0.0; dhwMeterLastMs = 0; dhwMeterSeeded = false; dhwMeterSavedL = 0.0; dhwMeterSavedMs = 0;
  dhwMeterResetPending = false;
  clearMQTTConfigDone(); clearMQTTConfigPending();
  g_pubs.clear();
  settings.mqtt.bEnable = true;
  state.flash.bESPactive = state.flash.bPICactive = false;
  LittleFS.files.clear(); LittleFS.writes = 0; LittleFS.failedCommits = 0; LittleFS.failNextCommit = false;
  LittleFSmounted = true;
  g_httpStatus = 0; g_httpBody.clear(); g_cors = false;
  g_ms = 5000;
}
static void takeSamples() {   // a MsgID 19 sample, then 10 s at 6 L/min: 1.0 L
  updateDHWWaterMeter(6.0f, g_ms);
  g_ms += 10000;
  updateDHWWaterMeter(6.0f, g_ms);
}
static void bootWithFile(const char* content) {   // setup(): the file is restored
  freshBoot();
  LittleFS.files[kFile] = content;
  loadDHWWaterMeter();
}
// A power cut and the next boot: the meter's RAM is lost, the file and what Home
// Assistant already received (g_pubs) stay.
static void powerCutAndBoot() {
  dhwWaterTotalL = 0.0; dhwMeterLastMs = 0; dhwMeterSeeded = false; dhwMeterSavedL = 0.0; dhwMeterSavedMs = 0;
  dhwMeterResetPending = false;
  state.flash.bESPactive = state.flash.bPICactive = false;
  LittleFSmounted = true;
  loadDHWWaterMeter();
}
// Every dhw_water_total payload published so far, in order: what Home Assistant saw.
static std::string sequence() {
  std::string s;
  for (const Pub& p : g_pubs) s += (s.empty() ? "" : " -> ") + p.payload;
  return s.empty() ? std::string("(none)") : s;
}

static char g_got[640];

// ---- D: the discovery row and its announce -------------------------------------------------
static void caseD1() {
  const size_t rows = sizeof(mqttHaSensors) / sizeof(mqttHaSensors[0]);
  const uint16_t idx = readSensorIndex(OTGWdhwmeterid);
  bool rowOk = false, single = false;
  std::string lbl, name, dc, unit, sc, icon;
  uint8_t flags = 0xFF; bool enabled = false; bool cat = true;
  if (idx != MQTT_HA_INDEX_NONE && idx < MQTT_HA_SENSOR_COUNT) {
    const MqttHaSensorCfg c = readSensorCfg(idx);
    rowOk = (c.id == OTGWdhwmeterid);
    single = (idx + 1u >= MQTT_HA_SENSOR_COUNT) || (readSensorCfg(idx + 1).id != OTGWdhwmeterid);
    lbl = str(c.label); name = str(c.friendlyName); dc = str(haDeviceClassStr(c.deviceClass));
    unit = str(haUnitStr(c.unit)); sc = str(haStateClassStr(c.stateClass)); icon = str(haIconStr(c.icon));
    flags = c.flags; enabled = c.enabledByDefault; cat = (haEntityCatStr(c.entityCat) != nullptr);
  }
  // Every id's index entry must point at that id's first row (the evaluate.py HA-DISC gate,
  // here against the compiled table).
  int badIndex = 0;
  for (int id = 0; id < 256; id++) {
    uint16_t first = MQTT_HA_INDEX_NONE;
    for (uint16_t i = 0; i < MQTT_HA_SENSOR_COUNT; i++) if (readSensorCfg(i).id == id) { first = i; break; }
    if (readSensorIndex((uint8_t)id) != first) badIndex++;
  }
  std::snprintf(g_got, sizeof g_got,
                "rows=%zu COUNT=%u index[241]=%u row id ok=%d single=%d badIndex=%d | label=%s name=%s "
                "device_class=%s unit=%s state_class=%s icon=mdi:%s flags=0x%02X enabled=%d entity_category=%s",
                rows, (unsigned)MQTT_HA_SENSOR_COUNT, (unsigned)idx, (int)rowOk, (int)single, badIndex, lbl.c_str(), name.c_str(),
                dc.c_str(), unit.c_str(), sc.c_str(), icon.c_str(), flags, (int)enabled, cat ? "set" : "none");
  verdict("D1", "table: COUNT equals the row count, index[241] points at the one dhw_water_total row, fields as on 1.x",
          rows == MQTT_HA_SENSOR_COUNT && rowOk && single && badIndex == 0 && lbl == "dhw_water_total" &&
          name == "DHW_Water_Total" && dc == "water" && unit == "L" && sc == "total_increasing" && icon == "water" &&
          flags == 0x00 && enabled && !cat,
          g_got);
}

static void caseD2() {
  const uint16_t idx = readSensorIndex(19);
  int rows = 0, same = 0;
  std::string dc, unit, sc;
  for (uint16_t i = idx; idx != MQTT_HA_INDEX_NONE && i < MQTT_HA_SENSOR_COUNT && readSensorCfg(i).id == 19; i++) {
    const MqttHaSensorCfg c = readSensorCfg(i);
    rows++;
    dc = str(haDeviceClassStr(c.deviceClass)); unit = str(haUnitStr(c.unit)); sc = str(haStateClassStr(c.stateClass));
    if (dc == "volume_flow_rate" && unit == "l/min" && sc == "measurement") same++;
  }
  std::snprintf(g_got, sizeof g_got, "MsgID 19 rows=%d, unchanged=%d (device_class=%s unit=%s state_class=%s)",
                rows, same, dc.c_str(), unit.c_str(), sc.c_str());
  verdict("D2", "the MsgID 19 rate sensor rows are unchanged (volume_flow_rate, l/min, measurement)",
          rows == 2 && same == 2, g_got);
}

static void caseD3() {
  // deviceForOTId() (MQTTstuff.ino) names the device when the config is published;
  // topoDeviceForPseudoId() (MQTTHaDiscovery.cpp) is its copy for the topology cleanup,
  // which must find the same uniq_id to clear it.
  const HaDevice d = deviceForOTId(OTGWdhwmeterid);
  const HaDevice t = topoDeviceForPseudoId(OTGWdhwmeterid);
  std::snprintf(g_got, sizeof g_got, "deviceForOTId(241)=%d topoDeviceForPseudoId(241)=%d (Sensors=%d)",
                (int)d, (int)t, (int)HaDevice::Sensors);
  verdict("D3", "241 routes to the Sensors device in both maps: uniq_id <nodeId>-sensors_dhw_water_total on every engine",
          d == HaDevice::Sensors && t == HaDevice::Sensors, g_got);
}

static void caseD4() {
  freshBoot();
  publishNonOTDiscoveryConfigs();                  // startMQTT() / broker restart
  publishDHWWaterMeter();                          // doTaskEvery60s()
  std::snprintf(g_got, sizeof g_got, "no sample yet: pending(241)=%d pending(242)=%d, state publishes=%zu",
                (int)pending(241), (int)pending(242), g_pubs.size());
  verdict("D4", "boot path without a MsgID 19 sample: 241 not queued (just in time, ADR-182), 242 is; no state published",
          !pending(241) && pending(242) && g_pubs.empty(), g_got);
}

static void caseD5() {
  freshBoot();
  markAllMQTTConfigPending();                      // force republish / settings save / daily heal
  std::snprintf(g_got, sizeof g_got, "no sample yet: pending(241)=%d, pending(19)=%d, pending(242)=%d",
                (int)pending(241), (int)pending(19), (int)pending(242));
  verdict("D5", "markAll without a MsgID 19 sample: 241 not queued (ADR-182), while 19 and 242 are",
          !pending(241) && pending(19) && pending(242), g_got);
}

static void caseD6() {
  bootWithFile("{\"litres\":1234.500}");           // restored, no sample on this boot
  publishNonOTDiscoveryConfigs();
  publishDHWWaterMeter();
  std::snprintf(g_got, sizeof g_got, "restored %.1f L only: state publishes=%zu, pending(241)=%d",
                dhwWaterTotalL, g_pubs.size(), (int)pending(241));
  verdict("D6", "a restored total without a sample on this boot: neither the entity nor its state is published",
          near(dhwWaterTotalL, 1234.5) && g_pubs.empty() && !pending(241), g_got);
}

static void caseD7() {
  freshBoot();
  takeSamples();
  publishDHWWaterMeter();                          // doTaskEvery60s(): the first one after a sample
  const bool viaPublish = pending(241);
  const Pub p = g_pubs.empty() ? Pub{"", "", true} : g_pubs.back();
  clearMQTTConfigPending(); publishNonOTDiscoveryConfigs();
  const bool viaBoot = pending(241);
  clearMQTTConfigPending(); markAllMQTTConfigPending();
  const bool viaMarkAll = pending(241);
  std::snprintf(g_got, sizeof g_got, "after a sample: publish %s=%s retain=%d (count %zu); pending via the 60 s publish=%d boot=%d markAll=%d",
                p.topic.c_str(), p.payload.c_str(), (int)p.retain, g_pubs.size(), (int)viaPublish, (int)viaBoot, (int)viaMarkAll);
  verdict("D7", "after a MsgID 19 sample: the 60 s publish sends dhw_water_total=1.0 (not retained) and queues 241; both republish paths queue it too",
          g_pubs.size() == 1 && p.topic == "dhw_water_total" && p.payload == "1.0" && !p.retain && viaPublish && viaBoot && viaMarkAll,
          g_got);
}

static void caseD12() {
  // The drip published 241 (done set, pending cleared). Every later 60 s publish sends the
  // state but must not queue the config again, or the gateway republishes it every minute.
  freshBoot();
  takeSamples();
  publishDHWWaterMeter();
  setMQTTConfigDone(OTGWdhwmeterid); clearMQTTConfigPending();
  g_ms += 60000;
  publishDHWWaterMeter();
  std::snprintf(g_got, sizeof g_got, "after the drip published 241: next 60 s publish queued it again=%d, state publishes=%zu",
                (int)pending(241), g_pubs.size());
  verdict("D12", "a published config is not queued again by the 60 s publish, which keeps sending the state",
          !pending(241) && g_pubs.size() == 2, g_got);
}

static void caseD8() {
  // The drip published the config (done set, pending cleared). Each republish path clears
  // done and queues 241 again. The broker restart path is the call sequence of
  // startMQTT() and onMqttConnect(), which the harness also checks statically (every
  // clearMQTTConfigDone() call is followed by a non-OT re-queue).
  freshBoot();
  takeSamples();
  setMQTTConfigDone(OTGWdhwmeterid); clearMQTTConfigPending();
  markAllMQTTConfigPending();
  const bool byMarkAll = pending(241) && !getMQTTConfigDone(OTGWdhwmeterid);
  setMQTTConfigDone(OTGWdhwmeterid); clearMQTTConfigPending();
  clearMQTTConfigDone(); clearMQTTConfigPending(); publishNonOTDiscoveryConfigs();
  const bool byBrokerRestart = pending(241) && !getMQTTConfigDone(OTGWdhwmeterid);
  std::snprintf(g_got, sizeof g_got, "after the drip published 241: queued again by markAll=%d, by a broker restart=%d",
                (int)byMarkAll, (int)byBrokerRestart);
  verdict("D8", "a republish re-announces 241: markAll and the broker-restart sequence each queue it again",
          byMarkAll && byBrokerRestart, g_got);
}

static void caseD9() {
  freshBoot();
  takeSamples();
  settings.mqtt.bEnable = false;
  publishDHWWaterMeter();
  std::snprintf(g_got, sizeof g_got, "MQTT disabled: publishes=%zu pending(241)=%d", g_pubs.size(), (int)pending(241));
  verdict("D9", "MQTT disabled: nothing published or queued", g_pubs.empty() && !pending(241), g_got);
}

static void caseD10() {
  // A /dhw_water.json holding 1e21 L (an upload or a corrupted file), then boot, a MsgID 19
  // sample and the 60 s publish. Rendered with every integer digit, 1e21 needs 25 bytes.
  bootWithFile("{\"litres\":1e21}");              // setup()
  takeSamples();
  publishDHWWaterMeter();                          // doTaskEvery60s()
  const std::string payload = lastPayload();
  std::snprintf(g_got, sizeof g_got, "file 1e21 L, one sample, 60 s publish: payload %s", payload.c_str());
  verdict("D10", "a file holding 1e21 L is ignored at boot, so the first publish is the 1.0 L drawn since",
          g_pubs.size() == 1 && payload == "1.0", g_got);
}

static void caseD11() {
  // The payload buffer is 24 bytes. 1e10 L is the largest total the loader accepts;
  // 1e300 is not reachable through the loader or the accumulator and is here only to
  // show that no total writes past the buffer. A formatter that writes every integer
  // digit (dtostrf) puts about 300 bytes into it, which ends this process (/GS).
  freshBoot();
  takeSamples();
  dhwWaterTotalL = 1e10;
  publishDHWWaterMeter();
  const std::string atBound = lastPayload();
  dhwWaterTotalL = 1e300;
  publishDHWWaterMeter();
  const size_t hugeLen = g_pubs.size() == 2 ? g_pubs.back().payload.size() : 0;
  std::snprintf(g_got, sizeof g_got, "total 1e10 L -> payload %s; total 1e300 L -> payload length %zu (buffer 24 bytes)",
                atBound.c_str(), hugeLen);
  verdict("D11", "the publisher stays inside its 24-byte buffer: 1e10 L publishes 10000000000.0, 1e300 L at most 23 characters",
          g_pubs.size() == 2 && atBound == "10000000000.0" && hugeLen > 0 && hugeLen <= 23, g_got);
}

// ---- X: the reset (ADR-176 Q6) ---------------------------------------------------------------
static void caseX1() {
  bootWithFile("{\"litres\":50.000}");
  takeSamples();                                   // 51 L
  publishDHWWaterMeter();                          // 51.0
  queueDHWWaterMeterReset();                       // REST or MQTT: only asks
  const double totalQueued = dhwWaterTotalL, fileQueued = fileLitres();
  const size_t pubsQueued = g_pubs.size(); const int writesQueued = LittleFS.writes;
  handlePendingDHWWaterMeterReset();               // loop()
  const double total = dhwWaterTotalL, file = fileLitres(), savedMark = dhwMeterSavedL;
  const size_t pubsAfter = g_pubs.size(); const int writesAfter = LittleFS.writes;
  const Pub p = g_pubs.back();
  handlePendingDHWWaterMeterReset();               // the next loop() pass
  std::snprintf(g_got, sizeof g_got,
                "queued: total=%.1f file=%.1f publishes=%zu writes=%d | loop pass: total=%.1f file=%.3f saved mark=%.1f "
                "writes=%d publishes=%zu last=%s=%s retain=%d | second pass: writes=%d publishes=%zu",
                totalQueued, fileQueued, pubsQueued, writesQueued, total, file, savedMark, writesAfter, pubsAfter,
                p.topic.c_str(), p.payload.c_str(), (int)p.retain, LittleFS.writes, g_pubs.size());
  verdict("X1", "reset: the queue alone changes nothing; one loop pass zeroes RAM and file and publishes 0.0 once",
          near(totalQueued, 51.0) && near(fileQueued, 50.0, 0.0005) && pubsQueued == 1 && writesQueued == 0 &&
          total == 0.0 && near(file, 0.0, 0.0005) && savedMark == 0.0 && writesAfter == 1 && pubsAfter == 2 &&
          p.topic == "dhw_water_total" && p.payload == "0.0" && !p.retain &&
          LittleFS.writes == 1 && g_pubs.size() == 2,
          g_got);
}

static void caseX2() {
  bootWithFile("{\"litres\":50.000}");
  takeSamples();                                   // 51 L, last sample at g_ms
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  g_ms += 10000; updateDHWWaterMeter(6.0f, g_ms);  // the next sample, 10 s later
  const double afterSample = dhwWaterTotalL;
  publishDHWWaterMeter();
  const int before = LittleFS.writes;
  saveDHWWaterMeterIfDue(g_ms);                    // the 60 s task: 1 L unsaved, not due
  std::snprintf(g_got, sizeof g_got, "after the reset, one 6 L/min sample 10 s later: total=%.4f L, publish=%s, tick writes=%d",
                afterSample, lastPayload().c_str(), LittleFS.writes - before);
  verdict("X2", "the next sample accumulates from 0 (1.0 L, not 52.0 L) and the write rule runs as usual",
          near(afterSample, 1.0) && lastPayload() == "1.0" && LittleFS.writes == before, g_got);
}

static void caseX3() {
  bootWithFile("{\"litres\":1234.500}");           // no MsgID 19 sample on this boot
  publishDHWWaterMeter();
  const size_t beforeReset = g_pubs.size();
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  const size_t afterReset = g_pubs.size();
  const std::string resetPayload = lastPayload();
  const double file = fileLitres();
  publishDHWWaterMeter();                          // the next 60 s tick, still no sample
  const size_t afterTick = g_pubs.size();
  takeSamples();
  publishDHWWaterMeter();
  std::snprintf(g_got, sizeof g_got,
                "no sample: publishes before reset=%zu, after reset=%zu (%s, file %.3f L), after the next tick=%zu; "
                "after a sample: %s",
                beforeReset, afterReset, resetPayload.c_str(), file, afterTick, lastPayload().c_str());
  verdict("X3", "a reset before any MsgID 19 sample publishes 0.0 once; the 60 s publish then waits for a sample, from 0",
          beforeReset == 0 && afterReset == 1 && resetPayload == "0.0" && near(file, 0.0, 0.0005) && afterTick == 1 &&
          g_pubs.size() == 2 && lastPayload() == "1.0",
          g_got);
}

static void caseX4() {
  // The reset's commit fails silently (a full filesystem): RAM is 0, the file keeps the
  // 50 L it had. Phase A: the next 60 s tick writes the 0. Phase B: a restart comes first.
  bootWithFile("{\"litres\":50.000}");
  takeSamples(); saveDHWWaterMeterIfDue(g_ms);     // 1 L unsaved: not due, the file keeps 50 L
  LittleFS.failNextCommit = true;
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  const double fileAfterFail = fileLitres(), markAfterFail = dhwMeterSavedL;
  const std::string payload = lastPayload();
  g_ms += 60000;
  const bool due = dhwWaterMeterSaveDue(g_ms);
  saveDHWWaterMeterIfDue(g_ms);                    // the next 60 s tick
  const double fileAfterTick = fileLitres();
  bootWithFile("{\"litres\":50.000}");
  takeSamples();
  LittleFS.failNextCommit = true;
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  flushDHWWaterMeter();                            // doRestart()
  const double fileAfterFlush = fileLitres();
  std::snprintf(g_got, sizeof g_got,
                "failed reset commit: file=%.3f L saved mark=%.1f L published=%s; A: due=%d, tick -> file=%.3f L | "
                "B: restart flush -> file=%.3f L",
                fileAfterFail, markAfterFail, payload.c_str(), (int)due, fileAfterTick, fileAfterFlush);
  verdict("X4", "a reset whose file write fails is written by the next 60 s tick or by the restart flush",
          near(fileAfterFail, 50.0, 0.0005) && near(markAfterFail, 50.0) && payload == "0.0" && due &&
          near(fileAfterTick, 0.0, 0.0005) && near(fileAfterFlush, 0.0, 0.0005),
          g_got);
}

static void caseX5() {
  bootWithFile("{\"litres\":50.000}");
  takeSamples();
  settings.mqtt.bEnable = false;
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  std::snprintf(g_got, sizeof g_got, "MQTT disabled: total=%.1f file=%.3f publishes=%zu", dhwWaterTotalL, fileLitres(), g_pubs.size());
  verdict("X5", "MQTT disabled: the reset still zeroes RAM and file, and publishes nothing",
          dhwWaterTotalL == 0.0 && near(fileLitres(), 0.0, 0.0005) && g_pubs.empty(), g_got);
}

static void caseX6() {
  // The reset waits while the file cannot be written, so RAM, file and the published 0
  // change together. A: a reset asked during a PIC flash, then a power cut before the
  // flash ends (the review's split: RAM 0, file 5000 L, 0.0 published, and after the
  // reboot Home Assistant counts the restored total as new water). B: the same reset,
  // and the flash ends. C: LittleFS unavailable (a failed health probe clears
  // LittleFSmounted). D: a filesystem upload (bESPactive), where LittleFSmounted stays
  // true although the upload unmounted LittleFS.
  bootWithFile("{\"litres\":5000.000}");
  takeSamples();                                   // 5001 L
  publishDHWWaterMeter();                          // Home Assistant has 5001.0
  state.flash.bPICactive = true;
  queueDHWWaterMeterReset();
  for (int pass = 0; pass < 3; pass++) handlePendingDHWWaterMeterReset();   // three loop passes during the flash
  const double ramA = dhwWaterTotalL, fileA = fileLitres();
  powerCutAndBoot();                               // the flash never finished
  takeSamples();                                   // +1 L on the new boot
  publishDHWWaterMeter();
  const std::string seqA = sequence();
  const bool okA = ramA == 5001.0 && near(fileA, 5000.0, 0.0005) && seqA == "5001.0 -> 5001.0";

  bootWithFile("{\"litres\":5000.000}");
  takeSamples();
  publishDHWWaterMeter();
  state.flash.bPICactive = true;
  queueDHWWaterMeterReset();
  for (int pass = 0; pass < 3; pass++) handlePendingDHWWaterMeterReset();
  const double ramHeld = dhwWaterTotalL, fileHeld = fileLitres();
  const std::string seqHeld = sequence();
  const int writesHeld = LittleFS.writes;
  state.flash.bPICactive = false;                  // the flash ends
  handlePendingDHWWaterMeterReset();               // the next loop pass
  const std::string seqB = sequence();
  const bool okB = ramHeld == 5001.0 && near(fileHeld, 5000.0, 0.0005) && seqHeld == "5001.0" && writesHeld == 0 &&
                   dhwWaterTotalL == 0.0 && near(fileLitres(), 0.0, 0.0005) && LittleFS.writes == 1 && seqB == "5001.0 -> 0.0";
  const double ramB = dhwWaterTotalL, fileB = fileLitres();

  bootWithFile("{\"litres\":7.000}");
  LittleFSmounted = false;
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  const bool heldC = dhwWaterTotalL == 7.0 && g_pubs.empty() && LittleFS.writes == 0;
  LittleFSmounted = true;
  handlePendingDHWWaterMeterReset();
  const bool ranC = dhwWaterTotalL == 0.0 && near(fileLitres(), 0.0, 0.0005) && sequence() == "0.0";

  bootWithFile("{\"litres\":7.000}");
  state.flash.bESPactive = true;
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  const bool heldD = dhwWaterTotalL == 7.0 && g_pubs.empty() && LittleFS.writes == 0;
  state.flash.bESPactive = false;
  handlePendingDHWWaterMeterReset();
  const bool ranD = dhwWaterTotalL == 0.0 && near(fileLitres(), 0.0, 0.0005) && sequence() == "0.0";

  std::snprintf(g_got, sizeof g_got,
                "A PIC flash, 3 passes, power cut: RAM at cut=%.1f file at cut=%.3f | published: %s | "
                "B flash ends: held RAM=%.1f file=%.3f writes=%d published %s, then RAM=%.1f file=%.3f published %s | "
                "C unmounted: held=%d, mounted again: ran=%d | D filesystem upload: held=%d, ended: ran=%d",
                ramA, fileA, seqA.c_str(), ramHeld, fileHeld, writesHeld, seqHeld.c_str(), ramB, fileB, seqB.c_str(),
                (int)heldC, (int)ranC, (int)heldD, (int)ranD);
  verdict("X6", "a reset asked during a flash or while LittleFS is unavailable waits; RAM, file and 0.0 then move together",
          okA && okB && heldC && ranC && heldD && ranD, g_got);
}

static void caseX7() {
  // A reset writes the file only when there is something to zero, and publishes 0 each time.
  // No file and no water: five resets on five loop passes. Then 1 L is drawn: one reset
  // writes, four more do not. Then a reset whose commit failed (RAM 0, file 50 L, saved
  // mark 50 L): the next reset writes the 0 at once.
  freshBoot();
  for (int i = 0; i < 5; i++) { queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset(); }
  const int writesIdle = LittleFS.writes; const size_t pubsIdle = g_pubs.size();
  const bool noFile = LittleFS.files.count(kFile) == 0;
  takeSamples();                                   // 1 L
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  const int writesAfterWater = LittleFS.writes;
  for (int i = 0; i < 4; i++) { queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset(); }
  const int writesMore = LittleFS.writes;
  bootWithFile("{\"litres\":50.000}");
  LittleFS.failNextCommit = true;
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  const double fileFailed = fileLitres();
  queueDHWWaterMeterReset(); handlePendingDHWWaterMeterReset();
  const double fileSecond = fileLitres();
  std::snprintf(g_got, sizeof g_got,
                "no water: 5 resets -> writes=%d publishes=%zu file absent=%d | 1 L drawn, 1 reset -> writes=%d, 4 more -> writes=%d | "
                "failed commit -> file=%.3f, next reset -> file=%.3f",
                writesIdle, pubsIdle, (int)noFile, writesAfterWater, writesMore, fileFailed, fileSecond);
  verdict("X7", "a reset writes only when the total or the file is not 0 (no flash cost for repeated resets) and publishes 0 every time",
          writesIdle == 0 && pubsIdle == 5 && noFile && writesAfterWater == 1 && writesMore == 1 &&
          near(fileFailed, 50.0, 0.0005) && near(fileSecond, 0.0, 0.0005), g_got);
}

// ---- Q: the REST and MQTT entry points ------------------------------------------------------
// Each call is followed by one loop() pass (handlePendingDHWWaterMeterReset()), so a case
// sees whether the entry point reached the reset, not only whether it raised a flag.
static bool resetReached(double totalBefore) {
  const size_t pubs = g_pubs.size();
  handlePendingDHWWaterMeterReset();
  return totalBefore > 0.0 && dhwWaterTotalL == 0.0 && g_pubs.size() == pubs + 1 && lastPayload() == "0.0";
}

static void caseQ1() {
  bootWithFile("{\"litres\":7.000}");
  restResetWaterTotalBranch(HTTP_POST);            // POST /api/v2/otgw/reset_water_total
  const int status = g_httpStatus; const std::string body = g_httpBody; const bool cors = g_cors;
  const bool reached = resetReached(7.0);
  std::snprintf(g_got, sizeof g_got, "POST: status=%d body=%s cors=%d; loop pass reached the reset=%d (file %.3f L)",
                status, body.c_str(), (int)cors, (int)reached, fileLitres());
  verdict("Q1", "REST POST answers 200 {\"status\":\"ok\",\"dhw_water_total\":0} and the next loop pass resets RAM, file and state",
          status == 200 && body == "{\"status\":\"ok\",\"dhw_water_total\":0}" && cors && reached &&
          near(fileLitres(), 0.0, 0.0005), g_got);
}

static void caseQ2() {
  const HTTPMethod methods[] = { HTTP_GET, HTTP_PUT, HTTP_DELETE };
  int refused = 0, reached = 0;
  for (HTTPMethod m : methods) {
    bootWithFile("{\"litres\":7.000}");
    restResetWaterTotalBranch(m);
    if (g_httpStatus == 405) refused++;
    handlePendingDHWWaterMeterReset();
    if (dhwWaterTotalL != 7.0 || !g_pubs.empty() || LittleFS.writes != 0) reached++;
  }
  std::snprintf(g_got, sizeof g_got, "GET, PUT, DELETE: answered 405=%d of 3, reached the reset=%d", refused, reached);
  verdict("Q2", "REST with any other method: 405 and no reset (POST only, like /api/v2/sat/reset_integral)",
          refused == 3 && reached == 0, g_got);
}

static void caseQ3() {
  const char* rests[] = { "/reset_water_total", "/RESET_WATER_TOTAL" };
  int reached = 0;
  for (const char* rest : rests) {
    bootWithFile("{\"litres\":7.000}");
    mqttOtgwBranch("otgw", rest, false);           // set/<node>/otgw/reset_water_total, any payload
    if (resetReached(7.0)) reached++;
  }
  std::snprintf(g_got, sizeof g_got, "set/<node>/otgw/reset_water_total and .../RESET_WATER_TOTAL: reached the reset=%d of 2", reached);
  verdict("Q3", "MQTT set/<node>/otgw/reset_water_total reaches the reset (the token is case-insensitive, like sat/)",
          reached == 2, g_got);
}

static void caseQ4() {
  struct T { const char* token; const char* rest; };
  const T others[] = { {"otgw", "/reset_water_totalX"}, {"otgw", "/reset_water_total_and_more_characters"},
                       {"otgw", "/reset_water"}, {"otgw", "/flush"}, {"otgw", ""}, {"otgw32", "/reset_water_total"},
                       {"sat", "/reset_water_total"} };
  int reached = 0;
  std::string which;
  for (const T& t : others) {
    bootWithFile("{\"litres\":7.000}");
    mqttOtgwBranch(t.token, t.rest, false);
    handlePendingDHWWaterMeterReset();
    if (dhwWaterTotalL != 7.0 || !g_pubs.empty()) { reached++; which += std::string(" ") + t.token + t.rest; }
  }
  std::snprintf(g_got, sizeof g_got, "other topics that reached the reset=%d of 7%s", reached, which.c_str());
  verdict("Q4", "no other topic resets: longer, shorter or other sub-commands, no sub-command, other command tokens",
          reached == 0, g_got);
}

// A retained reset is delivered again on every reconnect, so it must never reset: it
// would wipe the litres counted since the last live reset each time.
static void caseQ5() {
  bootWithFile("{\"litres\":7.000}");
  mqttOtgwBranch("otgw", "/reset_water_total", true);
  handlePendingDHWWaterMeterReset();
  const bool untouched = dhwWaterTotalL == 7.0 && g_pubs.empty() && fileLitres() == 7.0;
  std::snprintf(g_got, sizeof g_got, "retained set/<node>/otgw/reset_water_total: total=%.3f L, publishes=%u, file %.3f L",
                dhwWaterTotalL, (unsigned)g_pubs.size(), fileLitres());
  verdict("Q5", "a retained reset command is ignored: the total, the file and the published state stay as they were",
          untouched, g_got);
}

int main() {
  std::printf("== dhw_water_total discovery, publish and reset through the real code (TASK-1123) ==\n");
  caseD1(); caseD2(); caseD3(); caseD4(); caseD5(); caseD6(); caseD7(); caseD8(); caseD9(); caseD10(); caseD11(); caseD12();
  caseX1(); caseX2(); caseX3(); caseX4(); caseX5(); caseX6(); caseX7();
  caseQ1(); caseQ2(); caseQ3(); caseQ4(); caseQ5();
  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
