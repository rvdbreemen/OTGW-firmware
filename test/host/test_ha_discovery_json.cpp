// test/host/test_ha_discovery_json.cpp  (TASK-1202)
//
// Every Home Assistant discovery composer through the REAL code, every payload recorded at
// the publish boundary (mqttPublishRaw). test_ha_discovery_json.py takes from the revision
// under test MQTTHaDiscovery.cpp, MQTTstuff.h and boards.h verbatim (MQTTHaDiscovery.cpp is
// compiled as its own translation unit, as on the device) and slices into gen_*.inc:
//   - gen_types.inc: OTGWHardwareMode, DeviceSection, SensorsSection, OTdataStruct, the OT
//     enums, OTlookup_t, OTmap, PROGMEM_readAnything, CSTR(), three faux ids and the done
//     bitmap (Hardwaretypes.h, Devicetypes.h, Sensorstypes.h, OTGW-Core.h, helperStuff.h,
//     OTGW-firmware.h);
//   - gen_core.inc: messageIDToString() (OTGW-Core.ino), the override sensor labels;
//   - gen_dallas.inc: getDallasAddress() and the simulated 1-Wire addresses (sensors_ext.ino);
//   - gen_mqtt.inc: from MQTTstuff.ino the node id and namespaces, buildNamespace(), the done
//     bitmap, the session lock, buildDiscoveryContext(), deviceForOTId(),
//     doAutoConfigureMsgid(), sensorAutoConfigure(), the SAT zone and PV boost composers
//     with their helpers, and the BLE composer and remover (satBLEPublishHaDiscovery(),
//     satBLEUnpublishDiscovery()) with mqttJsonEscape();
//   - gen_override_ids.inc: the override sensor ids, read from doAutoConfigureMsgid().
// This file supplies the doubles at the edges (publish, MQTT connection, SAT zone state, the
// 1-Wire bus in part B) and the calls:
//   A: each composer called directly over its input domain, tagged with its own name;
//   B: doAutoConfigureMsgid() for every id 0..255 under four settings permutations;
//   C: clearTopologyDiscoveryForOTId() for every id, both stale schemes, both source modes.
// Output: a CTX line, one CALL line per call (ok=1 when the return value and the publish
// count are as expected) and one PUB line per publish, topic and payload hex-encoded. The
// .py validates every payload with a strict JSON parser.

#include "Arduino.h"      // ha_disc_shim: the platform
#include "platform.h"     // ha_disc_shim: platformFreeHeap()
#include "MQTTstuff.h"    // the revision's own header (generated directory)

#include <cstdarg>
#include <string>
#include <vector>

#include "gen_types.inc"

// ---- firmware globals the slices read ---------------------------------------------------
#define _VERSION "2.0.0-alpha.405+8fdc3b3 (03-10-2026)"   // version.h at 8795bacc0
struct {
  char                sHostname[41] = "OTGW";            // _HOSTNAME
  DeviceSection       device;
  MQTTSettingsSection mqtt;
  SensorsSection      sensors;
  struct { bool bPvBoostEnabled = false; } sat;
} settings;
struct {
  struct { OTGWHardwareMode eMode = HW_MODE_PIC; } hw;
  struct { bool bConnected = true; } mqtt;
} state;
typedef uint8_t DeviceAddress[8];                         // DallasTemperature.h

// ---- doubles at the edges of the code under test -----------------------------------------
#define DebugTf(...)     ((void)0)
#define MQTTDebugTf(...) ((void)0)
bool canPublishMQTT()         { return true; }
void feedWatchDog()           {}
void incPublishedTopicCount() {}
void PrintMQTTError()         {}
bool mqttIsConnected()        { return true; }
static struct { bool connected() const { return true; } } MQTTclient;   // espMqttClient, connected
struct IPAddress {};
static IPAddress MQTTbrokerIP;
static bool isValidIP(const IPAddress &) { return true; }
static void configSensors() {}       // part B: no 1-Wire sensors; part A runs the Dallas path
uint8_t satGetMaxZones() { return 4; }                                    // SAT_MAX_ZONES
bool satShouldDiscoverZone(uint8_t zoneIndex) { return zoneIndex < 4; }   // all four zones in use

// The publish boundary: every payload, with the composer that was called.
struct Pub { std::string tag, topic, payload; bool retain; };
static std::vector<Pub> g_pubs;
static const char *g_tag = "(none)";
bool mqttPublishRaw(const char *topic, const uint8_t *payload, size_t len, bool retain) {
  g_pubs.push_back({g_tag, topic ? topic : "",
                    payload ? std::string(reinterpret_cast<const char *>(payload), len) : std::string(), retain});
  return true;
}

#include "gen_core.inc"
#include "gen_dallas.inc"
#include "gen_mqtt.inc"
#include "gen_override_ids.inc"

// ---- recorder -----------------------------------------------------------------------------
static const long ANY = -1, RET_IS_PUBS = -2;
static int  g_seq = 0, g_bad = 0;
static char g_args[320];

static const char *argsf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const int n = std::vsnprintf(g_args, sizeof(g_args), fmt, ap);
  va_end(ap);
  if (n < 0 || static_cast<size_t>(n) >= sizeof(g_args)) { std::printf("HARNESS: call arguments too long\n"); std::exit(2); }
  return g_args;
}

static void hexOut(const std::string &s) {
  if (s.empty()) { std::fputs("-", stdout); return; }
  for (unsigned char c : s) std::printf("%02x", c);
}

static const char *hexLabel(PGM_P label) {        // a table row's full label, for the .py
  static char buf[2 * 120 + 1];
  size_t n = 0;
  for (const char *p = label; *p; p++, n += 2) {
    if (n + 2 >= sizeof(buf)) { std::printf("HARNESS: label longer than 120 characters\n"); std::exit(2); }
    std::snprintf(buf + n, 3, "%02x", static_cast<unsigned char>(*p));
  }
  buf[n] = '\0';
  return buf;
}

template <typename Fn>
static void call(const char *tag, const char *args, long wantRet, long wantPubs, Fn &&fn) {
  g_tag = tag;
  const long ret  = static_cast<long>(fn());
  const long pubs = static_cast<long>(g_pubs.size());
  const bool ok = (wantRet == ANY || (wantRet == RET_IS_PUBS ? ret == pubs : ret == wantRet)) &&
                  (wantPubs == ANY || pubs == wantPubs);
  if (!ok) g_bad++;
  std::printf("CALL %d %s %s ret=%ld pubs=%ld ok=%d\n", g_seq, tag, args, ret, pubs, ok ? 1 : 0);
  for (const Pub &p : g_pubs) {
    std::printf("PUB %d %s %d ", g_seq, p.tag.c_str(), p.retain ? 1 : 0);
    hexOut(p.topic);
    std::fputc(' ', stdout);
    hexOut(p.payload);
    std::fputc('\n', stdout);
  }
  g_pubs.clear();
  g_seq++;
  g_tag = "(none)";
}

static const char *devName(HaDevice d) {
  switch (d) {
    case HaDevice::Boiler:     return "Boiler";
    case HaDevice::Thermostat: return "Thermostat";
    case HaDevice::Gateway:    return "Gateway";
    case HaDevice::Esp:        return "Esp";
    case HaDevice::OtCore:     return "OtCore";
    case HaDevice::Sat:        return "Sat";
    case HaDevice::Sensors:    return "Sensors";
  }
  return "?";
}

// ---- A: each composer, called directly ------------------------------------------------------
// Context from buildDiscoveryContext(); the device as deviceForOTId() routes it, and a real OT
// id (0..127) a second time as Thermostat, the pass doAutoConfigureMsgid() adds in modern mode.
static void partA(OTGWHardwareMode engine, bool first) {
  state.hw.eMode = engine;
  const char *eng = (engine == HW_MODE_OT_DIRECT) ? "otd" : "pic";
  const int f = first ? 1 : 0;
  HaDiscoveryContext ctx = buildDiscoveryContext(first);

  for (uint16_t i = 0; i < MQTT_HA_SENSOR_COUNT; i++) {
    const MqttHaSensorCfg cfg = readSensorCfg(i);
    const HaDevice devs[2] = { deviceForOTId(cfg.id), HaDevice::Thermostat };
    for (int k = 0; k < (cfg.id <= 127 ? 2 : 1); k++) {
      ctx.device = devs[k];
      const char *a = argsf("engine=%s,first=%d,row=%u,id=%u,dev=%s,label=%s", eng, f, i, cfg.id,
                            devName(ctx.device), hexLabel(cfg.label));
      call("streamSensorDiscovery", a, 1, 1, [&] { return streamSensorDiscovery(cfg, ctx); });
      if (cfg.flags & MQTT_HA_FLAG_ANY_SOURCE)
        call("expandAndStreamSensorSources", a, 1, 2, [&] { return expandAndStreamSensorSources(cfg, ctx); });
    }
  }
  for (uint16_t i = 0; i < MQTT_HA_BINSENSOR_COUNT; i++) {
    const MqttHaBinSensorCfg cfg = readBinSensorCfg(i);
    const HaDevice devs[2] = { deviceForOTId(cfg.id), HaDevice::Thermostat };
    for (int k = 0; k < (cfg.id <= 127 ? 2 : 1); k++) {
      ctx.device = devs[k];
      call("streamBinarySensorDiscovery",
           argsf("engine=%s,first=%d,row=%u,id=%u,dev=%s,label=%s", eng, f, i, cfg.id, devName(ctx.device),
                 hexLabel(cfg.label)), 1, 1,
           [&] { return streamBinarySensorDiscovery(cfg, ctx); });
    }
  }

  ctx.device = HaDevice::Thermostat;
  for (uint8_t c = 0; c <= 2; c++)          // 2 is out of range: no publish
    call("streamClimateDiscovery", argsf("engine=%s,first=%d,climate=%u", eng, f, c), c <= 1, c <= 1,
         [&] { return streamClimateDiscovery(c, ctx); });
  call("streamNumberDiscovery", argsf("engine=%s,first=%d", eng, f), 1, 1, [&] { return streamNumberDiscovery(ctx); });

  ctx.device = HaDevice::Gateway;
  for (uint8_t id : kOverrideIds)
    call("streamOverrideSensorDiscovery", argsf("engine=%s,first=%d,id=%u", eng, f, id), 1, 1,
         [&] { return streamOverrideSensorDiscovery(ctx, id, messageIDToString(static_cast<OTLibMessageID>(id))); });
  call("streamOverrideSensorDiscovery", argsf("engine=%s,first=%d,id=27", eng, f), 0, 0,   // 27: the number entity
       [&] { return streamOverrideSensorDiscovery(ctx, 27, messageIDToString(static_cast<OTLibMessageID>(27))); });

  ctx.device = HaDevice::Sat;
  for (uint8_t s = 0; s <= 13; s++)         // 13 is out of range
    call("streamSatSwitchDiscovery", argsf("engine=%s,first=%d,switch=%u", eng, f, s), s <= 12, s <= 12,
         [&] { return streamSatSwitchDiscovery(s, ctx); });
  for (uint8_t s = 0; s <= 1; s++)          // 1 is out of range
    call("streamSatSelectDiscovery", argsf("engine=%s,first=%d,select=%u", eng, f, s), s == 0, s == 0,
         [&] { return streamSatSelectDiscovery(s, ctx); });

  ctx.device = HaDevice::Gateway;
  call("streamButtonDiscovery", argsf("engine=%s,first=%d", eng, f), 1, 1, [&] { return streamButtonDiscovery(ctx); });
  for (uint8_t s = 0; s <= 8; s++)          // 8 is out of range
    call("streamSelectDiscovery", argsf("engine=%s,first=%d,select=%u", eng, f, s), s <= 7, s <= 7,
         [&] { return streamSelectDiscovery(s, ctx); });

  // SAT zones and PV boost (MQTTstuff.ino): 4 zones x 3 entities, 6 PV controls, and 3 PV
  // telemetry entities while PV boost is on. The composer clears isFirstEntity as it goes,
  // so each call gets a fresh context.
  for (int pv = 0; pv <= 1; pv++) {
    settings.sat.bPvBoostEnabled = (pv != 0);
    HaDiscoveryContext zctx = buildDiscoveryContext(first);
    zctx.device = HaDevice::Sat;
    call("streamSatZoneDiscovery", argsf("engine=%s,first=%d,pv_boost=%d", eng, f, pv), 1, 4 * 3 + 6 + (pv ? 3 : 0),
         [&] { return streamSatZoneDiscovery(zctx); });
  }
  settings.sat.bPvBoostEnabled = false;
}

// Dallas: the firmware's own entry, sensorAutoConfigure(), as configSensors() calls it, for the
// simulated sensor addresses in both address formats.
static void partADallas() {
  for (int legacy = 0; legacy <= 1; legacy++) {
    settings.sensors.bLegacyFormat = (legacy != 0);
    for (int s = 0; s < SIM_SENSOR_COUNT; s++) {
      DeviceAddress addr;
      std::memcpy(addr, DallasSimDeviceAddresses[s], sizeof(addr));
      const char *address = getDallasAddress(addr);
      call("streamDallasSensorDiscovery", argsf("legacy_format=%d,sensor=%d,address=%s", legacy, s, address), ANY, 1,
           [&] { sensorAutoConfigure(OTGWdallasdataid, false, address); return true; });
    }
  }
  settings.sensors.bLegacyFormat = false;
}

// BLE sensors (MQTTstuff.ino): satBLEPublishHaDiscovery() publishes four sensor configs per MAC,
// with and without a user label (one with a quote and a backslash, which mqttJsonEscape() must
// escape); satBLEUnpublishDiscovery() removes them with four empty retained payloads. The
// composer formats its PROGMEM strings with "%S" through snprintf_P, which is snprintf on the
// ESP32 core and here, so the host C library decides what "%S" does: the .py reports this
// probe apart from the gate.
static void partABle() {
  const char *labels[3] = { nullptr, "Woonkamer", "Zolder \"oost\" \\ 2" };
  for (int l = 0; l < 3; l++)
    call("satBLEPublishHaDiscovery", argsf("label=%d", l), ANY, ANY,
         [&] { return satBLEPublishHaDiscovery("A4C138112233", "A4:C1:38:11:22:33", labels[l]); });
  call("satBLEUnpublishDiscovery", "mac=A4C138112233", ANY, 4,
       [&] { satBLEUnpublishDiscovery("A4C138112233"); return true; });
}

// ---- B: the discovery dispatcher for every id ---------------------------------------------
struct Perm {
  const char *name;
  bool legacyMode, legacyTopics, separateSources, dhwTank, pvBoost;
  OTGWHardwareMode engine;
};
static const Perm kPerms[] = {
  // name                    legacy topology, legacy names, sources, DHW tank, PV boost, engine
  {"modern_pic",             false, false, false, true,  true,  HW_MODE_PIC},
  {"legacy_otd_sources",     true,  true,  true,  false, false, HW_MODE_OT_DIRECT},
  {"modern_oldnames_sources", false, true, true,  true,  false, HW_MODE_OT_DIRECT},
  {"legacy_newnames_pic",    true,  false, false, false, true,  HW_MODE_PIC},
};

static void partB() {
  for (const Perm &p : kPerms) {
    settings.mqtt.bLegacyMode = p.legacyMode;
    settings.mqtt.bUseLegacyOtTopics = p.legacyTopics;
    settings.mqtt.bSeparateSources = p.separateSources;
    settings.sat.bPvBoostEnabled = p.pvBoost;
    OTcurrentSystemState.SlaveConfigMemberIDcode = p.dhwTank ? 0x0800 : 0;   // MsgID 3 HB3: storage tank
    state.hw.eMode = p.engine;
    for (int id = 0; id <= 255; id++)      // id 0 opens the cycle, so it carries the full device block
      call("doAutoConfigureMsgid", argsf("perm=%s,id=%d", p.name, id), ANY, ANY,
           [&] { return doAutoConfigureMsgid(static_cast<byte>(id), id == 0); });
  }
  settings.mqtt.bLegacyMode = settings.mqtt.bUseLegacyOtTopics = settings.mqtt.bSeparateSources = false;
  settings.sat.bPvBoostEnabled = false;
  OTcurrentSystemState.SlaveConfigMemberIDcode = 0;
  state.hw.eMode = HW_MODE_PIC;
}

// ---- C: topology clean-up publishes (empty retained payloads) ----------------------------------
static void partC() {
  for (int legacy = 0; legacy <= 1; legacy++)
    for (int sep = 0; sep <= 1; sep++)
      for (int id = 0; id <= 255; id++)
        call("clearTopologyDiscoveryForOTId", argsf("stale_legacy=%d,separate_sources=%d,id=%d", legacy, sep, id),
             RET_IS_PUBS, ANY, [&] {
               return clearTopologyDiscoveryForOTId(static_cast<uint8_t>(id), legacy != 0,
                                                    CSTR(settings.mqtt.sHaprefix), NodeId, sep != 0);
             });
}

// ---- D: the TASK-1201 one-time clear of the thermostat_ duplicates (empty retained payloads) ---
// Built only when the code under test defines the helper (the Python side passes the define).
static void partD() {
#ifdef HAS_THERMOSTAT_DUP_CLEAR
  for (int id = 0; id <= 255; id++)
    call("clearThermostatDupDiscoveryForOTId", argsf("id=%d", id), ANY, ANY, [&] {
      uint8_t cleared = 0;
      return clearThermostatDupDiscoveryForOTId(static_cast<uint8_t>(id), CSTR(settings.mqtt.sHaprefix),
                                                NodeId, &cleared);
    });
#endif
}

int main() {
  // startMQTT(): the node id and both namespaces from the default settings, with the unique id
  // getUniqueId() makes ("otgw-" and the 12 hex digits of the MAC).
  strlcpy(settings.mqtt.sUniqueid, "otgw-1020BA21B4F8", sizeof(settings.mqtt.sUniqueid));
  strlcpy(NodeId, CSTR(settings.mqtt.sUniqueid), sizeof(NodeId));
  buildNamespace(MQTTPubNamespace, sizeof(MQTTPubNamespace), CSTR(settings.mqtt.sTopTopic), "value", NodeId);
  buildNamespace(MQTTSubNamespace, sizeof(MQTTSubNamespace), CSTR(settings.mqtt.sTopTopic), "set", NodeId);
  std::printf("CTX prefix=%s pub=%s sub=%s node=%s\n", CSTR(settings.mqtt.sHaprefix), MQTTPubNamespace,
              MQTTSubNamespace, NodeId);

  for (OTGWHardwareMode engine : {HW_MODE_PIC, HW_MODE_OT_DIRECT})
    for (int first = 1; first >= 0; first--) partA(engine, first != 0);
  partADallas();
  partABle();
  partB();
  partC();
  partD();
  std::printf("END calls=%d not_ok=%d\n", g_seq, g_bad);
  return g_bad ? 1 : 0;
}
