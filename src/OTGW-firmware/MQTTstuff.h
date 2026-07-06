// MQTTstuff.h -- HA MQTT Discovery data layer
// Compact PROGMEM arrays + streaming constructor declarations
// Part of OTGW-firmware, GNU GPLv3
//
// Copyright (c) 2021-2026 Robert van den Breemen
//
// Hand-written header: enums and structs are stable across cfg changes.
// Companion files: MQTTstuff.ino (MQTT engine) and MQTTHaDiscovery.cpp
// (PROGMEM data arrays + streaming discovery constructors).

#pragma once
#include <pgmspace.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>    // snprintf — snprintf_P expands to snprintf on ESP32 (writeRamEscaped \uXXXX)
#include <string.h>   // memcpy / strlen — MqttJsonWriter WRITE buffer + readSensorCfg memcpy_P
#include <boards.h>   // HAS_PIC / HAS_DIRECT_OT capability flags — needed in BOTH TUs
                      // (the standalone MQTTHaDiscovery.cpp does not pull OTGW-firmware.h,
                      // so it would otherwise see HAS_PIC undefined; the OT-Core device's
                      // user-facing name/suffix is rendered per hardware via these flags).

// MQTTstuff.h is included by both the main Arduino translation unit (via
// OTGW-firmware.h, which defines HOME_ASSISTANT_DISCOVERY_PREFIX) and by the
// standalone MQTTHaDiscovery.cpp (which does NOT include OTGW-firmware.h).
// Guard the constant so the second TU still compiles. Kept as a #define to
// preserve the original macro semantics; the guard makes the definition
// idempotent if OTGW-firmware.h has already provided it.
#ifndef HOME_ASSISTANT_DISCOVERY_PREFIX
#define HOME_ASSISTANT_DISCOVERY_PREFIX   "homeassistant"
#endif

// ============================================================================
// State + Settings sections (ADR-081 -- merged from former MQTTtypes.h)
// ============================================================================
//
// MQTTRuntimeSection and MQTTSettingsSection live here (inside MQTTstuff.h)
// because MQTT is a component that owns both machinery and state/settings
// structs, and per ADR-081 types merge into <Component>stuff.h when a
// stuff.h sibling exists. Accessed as state.mqtt.<field> and
// settings.mqtt.<field>.

struct MQTTRuntimeSection {    // state.mqtt -- MQTT broker connection state
  bool bConnected        = false;  // was statusMQTTconnection
  uint32_t iLastConnectedMs = 0;   // millis() when MQTT was last connected (for fallback detection)
};

// ADR-116: default heartbeat interval (s) used both as the fresh-install
// MQTTinterval default and as the target of the one-time 0 -> 60 migration.
#ifndef MQTT_DEFAULT_PUBLISH_INTERVAL_SEC
#define MQTT_DEFAULT_PUBLISH_INTERVAL_SEC 60
#endif

struct MQTTSettingsSection {
  bool    bEnable          = true;
  bool    bSecure          = false;
  char    sBroker[65]      = "homeassistant.local";
  uint16_t iBrokerPort     = 1883;
  char    sUser[41]        = "";
  char    sPasswd[41]      = "";
  char    sHaprefix[41]    = HOME_ASSISTANT_DISCOVERY_PREFIX;
  bool    bHaRebootDetect  = true;
  bool    bDiscoveryAutoVerify = true;  // ADR-062/TASK-349/351: daily automatic retained-discovery verification
  char    sTopTopic[41]    = "OTGW";
  // Format budget: "otgw-" (5) + up to 32-char device id + optional suffix.
  // The streaming HA discovery composer may prepend/append short fragments,
  // so keep headroom. Minimum 20 bytes is a hard lower bound.
  char    sUniqueid[41]    = "";  // Initialized in readSettings
  static_assert(sizeof(sUniqueid) >= 20, "sUniqueid must fit 'otgw-' + chipId");
  bool    bOTmessage       = false;
  bool    bOnChangePublishing = true; // On-change publishing: publish on change, heartbeat unchanged values every iInterval (ADR-116)
  uint16_t iInterval       = MQTT_DEFAULT_PUBLISH_INTERVAL_SEC; // Heartbeat interval (s) when on-change active; 0 = legacy publish-every-message
  bool    bSeparateSources = false; // ADR-040: publish source-specific topics
  bool    bLegacyPort25238Enabled = false;
  bool    bUseLegacyOtTopics = false;  // ADR-106: false (default) → publish new self-describing names (supports_*, fault_indication, ventilation_*, etc.). true → publish legacy OT-spec-derived names. Mutually exclusive — never both at the same time. Toggle triggers cleanup of the OTHER set's retained discovery topics.
  // TASK-648: umbrella 1.x.x compatibility mode. false (default) = modern
  // (HA-core five-device topology + {nodeId}-{device}-{label} unique_ids +
  // new self-describing OT-topic names). true = legacy 1.x.x. Subsumes
  // bUseLegacyOtTopics (kept as a deprecated load-time alias for one release).
  bool    bLegacyMode = false;
  // TASK-648 Task 6: persisted topology stamp. Tracks which topology scheme
  // was in effect the last time discovery was fully published. On the next
  // discovery cycle, if bLastPublishedLegacy != bLegacyMode a topology
  // migration is detected and stale config topics from the OLD scheme are
  // erased with empty retained publishes so HA removes those entities.
  // Updated (and persisted) only after the stale-topic drain completes.
  bool    bLastPublishedLegacy = false;
};

// ---------------------------------------------------------------------------
// PROGMEM-safe helpers -- shared between MQTTstuff.ino and the data layer.
//
// On ESP8266, standard strstr/strncmp may use word-aligned 32-bit reads.
// Unaligned reads from flash (0x402xxxxx) cause Exception (3) on Arduino
// Core 3.1.2+. These helpers use pgm_read_byte for safe byte access.
// ---------------------------------------------------------------------------
#ifndef MQTTHA_PGM_HELPERS_DEFINED
#define MQTTHA_PGM_HELPERS_DEFINED
inline int pgm_strncmp_PP(PGM_P s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        uint8_t c1 = pgm_read_byte(s1 + i);
        uint8_t c2 = static_cast<uint8_t>(s2[i]);
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == 0) return 0;
    }
    return 0;
}

inline char pgm_read_char(PGM_P p)
{
    return static_cast<char>(pgm_read_byte(p));
}
#endif

// strlcpy_P is not declared in ESP8266 Arduino Core 3.1.2. Provide a shared
// fallback for every TU that #includes MQTTstuff.h so sendMQTTDataPic and
// similar helpers compile on ESP8266 and ESP32 alike.
#ifndef strlcpy_P
inline size_t strlcpy_P(char *dst, PGM_P src, size_t size) {
  size_t srcLen = strlen_P(src);
  if (size > 0) {
    size_t n = (srcLen < size - 1) ? srcLen : (size - 1);
    memcpy_P(dst, src, n);
    dst[n] = '\0';
  }
  return srcLen;
}
#endif

// ---------------------------------------------------------------------------
// Enums -- uint8_t backed for minimal PROGMEM footprint
//
// Each enum value maps to a PROGMEM string via the corresponding
// haXxxStr() lookup function declared below.
// Names use snake_case to match the code generator output.
// ---------------------------------------------------------------------------

// HA device_class values found in mqttha.cfg sensor entries
enum class HaDeviceClass : uint8_t {
    none = 0,           // no device_class set
    temperature,        // "temperature"
    pressure,           // "pressure"
    humidity,           // "humidity"
    power,              // "power"
    power_factor,       // "power_factor"
    energy,             // "energy"
    carbon_dioxide,     // "carbon_dioxide"
    _count
};

// HA unit_of_measurement values found in mqttha.cfg
enum class HaUnit : uint8_t {
    none = 0,           // no unit / empty string
    degC,               // degrees Celsius
    deg,                // degrees
    percent,            // "%"
    bar,                // "bar"
    hPa,                // "hPa"
    l_min,              // "l/min"
    kW,                 // "kW"
    W,                  // "W"
    kWh,                // "kWh"
    uA,                 // micro-ampere
    Hz,                 // "Hz"
    rpm,                // "rpm"
    ppm,                // "ppm"
    mS,                 // "mS" (milliseconds, S0 pulse time)
    m_s,                // "m/s"
    mm,                 // "mm"
    s,                  // "s" (seconds)
    h,                  // "h" (hours)
    kW_percent,         // "kW/%" (MaxCapacity composite)
    bytes,              // "B" (bytes, used by heap-diag sensors)
    _count
};

// HA state_class values
enum class HaStateClass : uint8_t {
    none = 0,           // not set
    measurement,        // "measurement"
    total_increasing,   // "total_increasing"
    _count
};

// MDI icons -- one per logical function
enum class HaIcon : uint8_t {
    none = 0,               // use HA default (no explicit icon)
    // Sensor icons
    thermometer,            // mdi:thermometer
    gauge,                  // mdi:gauge
    water_percent,          // mdi:water-percent
    flash,                  // mdi:flash
    angle_acute,            // mdi:angle-acute
    lightning_bolt,         // mdi:lightning-bolt
    molecule_co2,           // mdi:molecule-co2
    percent_outline,        // mdi:percent-outline
    timer_outline,          // mdi:timer-outline
    counter,                // mdi:counter
    information_outline,    // mdi:information-outline
    fan,                    // mdi:fan
    current_ac,             // mdi:current-ac
    clock_outline,          // mdi:clock-outline
    pulse,                  // mdi:pulse
    // Binary sensor icons
    alert_circle,           // mdi:alert-circle
    fire,                   // mdi:fire
    radiator,               // mdi:radiator
    water_boiler,           // mdi:water-boiler
    snowflake,              // mdi:snowflake
    information,            // mdi:information
    toggle_switch,          // mdi:toggle-switch
    lan_connect,            // mdi:lan-connect
    checkbox_marked_circle, // mdi:checkbox-marked-circle
    // Climate / number
    thermostat_icon,        // mdi:thermostat
    thermometer_lines,      // mdi:thermometer-lines
    // New icons from mqttha_icons.cfg
    air_filter,             // mdi:air-filter
    alert_outline,          // mdi:alert-outline
    antenna,                // mdi:antenna
    arrow_expand_horizontal, // mdi:arrow-expand-horizontal
    calendar,               // mdi:calendar
    card_account_details,   // mdi:card-account-details
    cog,                    // mdi:cog
    console,                // mdi:console
    format_list_numbered,   // mdi:format-list-numbered
    history,                // mdi:history
    list_status,            // mdi:list-status
    remote,                 // mdi:remote
    solar_panel,            // mdi:solar-panel
    speedometer,            // mdi:speedometer
    tag,                    // mdi:tag
    tune_variant,           // mdi:tune-variant
    water,                  // mdi:water
    _count
};

// HA entity_category
enum class HaEntityCat : uint8_t {
    none = 0,           // default (not diagnostic, not config)
    diagnostic,         // "diagnostic"
    config,             // "config"
    _count
};

enum class HaBinaryPayload : uint8_t {
    on_off = 0,         // Home Assistant default (ON/OFF)
    true_false,         // "true" / "false"
    one_zero,           // 1 / 0
    _count
};

// ---------------------------------------------------------------------------
// Enum-to-string lookup functions -- return PGM_P (flash pointer)
// Returns nullptr for ::none values (caller should omit the JSON key).
// ---------------------------------------------------------------------------
PGM_P haDeviceClassStr(HaDeviceClass dc);
PGM_P haUnitStr(HaUnit u);
PGM_P haStateClassStr(HaStateClass sc);
PGM_P haIconStr(HaIcon icon);
PGM_P haEntityCatStr(HaEntityCat cat);

// ---------------------------------------------------------------------------
// Flag bit definitions
// ---------------------------------------------------------------------------
#ifndef MQTT_HA_FLAGS_DEFINED
#define MQTT_HA_FLAGS_DEFINED
constexpr uint8_t MQTT_HA_FLAG_SOURCE_SUFFIX        = 0x01;
constexpr uint8_t MQTT_HA_FLAG_SOURCE_NAME          = 0x02;
constexpr uint8_t MQTT_HA_FLAG_SOURCE_TOPIC_SEGMENT = 0x04;
constexpr uint8_t MQTT_HA_FLAG_IS_PIC_ENTRY         = 0x08;
constexpr uint8_t MQTT_HA_FLAG_IS_HA_CORE_ALIAS         = 0x10;  // ADR-106: published in new mode (default; settings.mqtt.bUseLegacyOtTopics=false). Skipped in legacy mode.
constexpr uint8_t MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS  = 0x20;  // ADR-106: legacy row that has an alias replacement. Skipped in new mode, published in legacy mode.
constexpr uint8_t MQTT_HA_FLAG_ANY_SOURCE                = 0x07;
#endif

// ---------------------------------------------------------------------------
// PIC subtree prefix -- stable public topic API (see ADR-065 / TASK-389).
// Entries flagged MQTT_HA_FLAG_IS_PIC_ENTRY publish and are discovered under
// <mqttPubTopic>/otgw-pic/<label>. Single source of truth for the subtree
// name so future renames or migrations touch exactly one location.
// Defined in MQTTstuff.ino.
// ---------------------------------------------------------------------------
extern const char kPicSubtreePrefix[] PROGMEM;

// ---------------------------------------------------------------------------
// Sensor discovery config -- one per mqttha.cfg sensor entry
// ---------------------------------------------------------------------------
struct MqttHaSensorCfg {
    uint8_t       id;              // OT message ID (0-255), or 245/246 for S0/Dallas
    uint8_t       flags;           // MQTT_HA_FLAG_* bits
    PGM_P         label;           // "TSet" -- for topic, stat_t, uniq_id
    PGM_P         friendlyName;    // "Control setpoint" -- for HA display name
    HaDeviceClass deviceClass;
    HaUnit        unit;
    HaStateClass  stateClass;
    HaIcon        icon;
    HaEntityCat   entityCat;
    bool          enabledByDefault;
    PGM_P         valueTemplate;       // optional custom value_template; null => "{{ value }}"
    PGM_P         jsonAttributesTopic; // optional relative topic path after <mqttPubTopic>/
};

// Binary sensor discovery config
struct MqttHaBinSensorCfg {
    uint8_t       id;              // OT message ID
    uint8_t       flags;           // MQTT_HA_FLAG_* bits
    PGM_P         label;           // "fault" -- for topic, stat_t, uniq_id
    PGM_P         friendlyName;    // "Fault" -- for HA display name
    HaIcon        icon;
    HaEntityCat   entityCat;
    bool          enabledByDefault;
    PGM_P         deviceClass;     // optional HA binary_sensor device_class
    HaBinaryPayload payload;
};

// ---------------------------------------------------------------------------
// PROGMEM arrays -- defined in MQTTHaDiscovery.cpp (generated from mqttha.cfg)
// Climate and Number discovery are handled by streaming functions directly.
// ---------------------------------------------------------------------------
extern const MqttHaSensorCfg    PROGMEM mqttHaSensors[];
extern const MqttHaBinSensorCfg PROGMEM mqttHaBinSensors[];

// OT ID -> first entry index lookup tables
extern const uint16_t PROGMEM mqttHaSensorIndex[256];
extern const uint16_t PROGMEM mqttHaBinSensorIndex[256];

// Entry counts
extern const uint16_t MQTT_HA_SENSOR_COUNT;
extern const uint16_t MQTT_HA_BINSENSOR_COUNT;
extern const uint16_t MQTT_HA_BINSENSOR_INDEXED_COUNT;  // ADR-105: first N rows covered by mqttHaBinSensorIndex[]; rows >= this are alias tail.

// ---------------------------------------------------------------------------
// PROGMEM read helpers
// ---------------------------------------------------------------------------
inline MqttHaSensorCfg readSensorCfg(uint16_t idx) {
    MqttHaSensorCfg cfg;
    memcpy_P(&cfg, &mqttHaSensors[idx], sizeof(cfg));
    return cfg;
}

inline MqttHaBinSensorCfg readBinSensorCfg(uint16_t idx) {
    MqttHaBinSensorCfg cfg;
    memcpy_P(&cfg, &mqttHaBinSensors[idx], sizeof(cfg));
    return cfg;
}

inline uint16_t readSensorIndex(uint8_t otId) {
    return pgm_read_word(&mqttHaSensorIndex[otId]);
}

inline uint16_t readBinSensorIndex(uint8_t otId) {
    return pgm_read_word(&mqttHaBinSensorIndex[otId]);
}

constexpr uint16_t MQTT_HA_INDEX_NONE = 0xFFFF;

// ---------------------------------------------------------------------------
// Cross-TU tuning constant -- exposed here so restAPI.ino and other callers
// can enforce the same heap floor as startDiscoveryVerification() without
// duplicating a magic number. Full ADR-062 tuning rationale lives in
// MQTTstuff.ino alongside the other VERIFICATION_* constants (kept local
// because only this one has external callers).
// ---------------------------------------------------------------------------
constexpr uint32_t VERIFICATION_MIN_HEAP_START = 6000;

// ---------------------------------------------------------------------------
// Discovery context -- runtime state passed to streaming functions
// ---------------------------------------------------------------------------

// ADR-124 (TASK-826) / ADR-140: HA device routing ordinal.
// Single-device topology (ADR-140): there is one HA device (bare nodeId). The
// HaDevice enum no longer selects a per-device identifier; it now only selects
// the entity SOURCE prefix (haSourcePrefix/haSourceNamePrefix in
// MQTTHaDiscovery.cpp) so OT-engine entities read "pic_"/"otd_" and the
// firmware-native groups read "esp_"/"sat_"/"sensors_". OtCore = the OpenTherm
// bus driver slot, rendered per hardware via ctx.otCoreName.
enum class HaDevice : uint8_t { Boiler = 0, Thermostat, Gateway, Esp, OtCore, Sat, Sensors };

// ADR-124 §2: the OT-Core device is one enum slot (HaDevice::OtCore) whose
// USER-FACING name/suffix is rendered per hardware so the model is obvious in
// HA — "pic" on a PIC build (HAS_PIC, classic OTGW), "ot-direct" on an OTGW32
// direct-OT build (HAS_DIRECT_OT). The fixed builds link exactly one bus
// driver, so the choice is compile-time. boards.h (included above) makes
// HAS_PIC visible in both TUs (the standalone MQTTHaDiscovery.cpp cannot see
// OTGW-firmware.h).
// On fixed boards HAS_PIC=1 (classic OTGW) or HAS_DIRECT_OT=1 (OTGW32); the
// compile-time macros below resolve correctly at build time.
// On the combo board (HAS_RUNTIME_HW_DETECT=1, ADR-127) both flags are set but
// only one bus is active at runtime. TASK-847 threads the runtime slug through
// HaDiscoveryContext.otCoreSuffix / .otCoreName so normal discovery emits the
// correct device identity per mode. The topology-clear path (topoDeviceName())
// still uses the compile-time macro; topics orphaned by a mode-switch are
// accepted as manual-cleanup (ADR-124 §alpha).
#if defined(HAS_PIC) && HAS_PIC
  #define HA_OTCORE_NAME    "pic"
  #define HA_OTCORE_SUFFIX  "-pic"
#else
  #define HA_OTCORE_NAME    "ot-direct"
  #define HA_OTCORE_SUFFIX  "-ot-direct"
#endif

struct HaDiscoveryContext {
    const char *nodeId;
    const char *hostname;
    const char *version;
    const char *mqttPubTopic;
    const char *mqttSubTopic;
    const char *haPrefix;
    const char *manufacturer;      // Hardware manufacturer (from settings.device) — legacy only
    const char *model;             // Hardware model (from settings.device) — legacy only
    bool        isFirstEntity;
    bool        legacyMode = false;           // settings.mqtt.bLegacyMode, threaded in (the .cpp TU cannot see globals)
    HaDevice    device = HaDevice::Esp;       // routing ordinal: selects the entity source prefix (ADR-140), not a device id
    // TASK-847: OtCore device suffix/name — set unconditionally in buildDiscoveryContext().
    // Fixed boards: compile-time HA_OTCORE_SUFFIX / HA_OTCORE_NAME.
    // Combo (HAS_RUNTIME_HW_DETECT=1): runtime from state.hw.eMode.
    PGM_P       otCoreSuffix;    // PROGMEM ptr: "-pic" or "-ot-direct"
    const char *otCoreName;      // RAM ptr: "pic" or "ot-direct"
    // Source template expansion (set per-source iteration)
    const char *sourceSuffix;
    const char *sourceName;
    const char *sourceTopicSegment;
};

// ---------------------------------------------------------------------------
// mqttPublishRaw -- the single MQTT publish chokepoint (TASK-865.7 / ADR-123).
//
// espMqttClient frames a publish atomically: it copies (topic + payload) into
// its Outbox at publish() time, then pumps the bytes onto the wire from loop().
// There is no streaming beginPublish/write/endPublish API as PubSubClient had,
// so every outbound publish in the firmware buffers its payload (already the
// case for the small per-message helpers; the discovery composer measures the
// payload, mallocs a transient heap buffer, fills it once, then calls this).
//
// On the ESP32-S3 the worst-case discovery payload is ~900 B — trivial against
// the DRAM budget — so the 128 B ESP8266-only chunker the old code carried is
// gone. payload may be nullptr when len==0 (empty retained publish = delete).
// Returns true when the publish was queued (packetId != 0), false otherwise
// (not connected / out of outbox memory). Defined in MQTTstuff.ino.
// ---------------------------------------------------------------------------
bool mqttPublishRaw(const char* topic, const uint8_t* payload, size_t len, bool retain);

// ---------------------------------------------------------------------------
// MqttJsonWriter -- dual-mode writer for composing HA discovery JSON
//
// In MEASURE mode: no output; just accumulates byteCount so the caller can
// size the publish buffer exactly.
// In WRITE mode: appends bytes into a caller-supplied heap buffer (buf/cap).
//
// The same composition function runs twice — first MEASURE to learn the
// length, then WRITE into a malloc'd buffer of that length — keeping the two
// passes perfectly in sync without duplicating the JSON structure logic.
// The filled buffer is then handed to mqttPublishRaw() as one atomic publish.
//
// Defined in the header so Arduino's auto-prototype generation knows the type
// before it generates forward declarations for functions that take a
// MqttJsonWriter& parameter.
// ---------------------------------------------------------------------------
struct MqttJsonWriter {
  enum Mode : uint8_t { MEASURE = 0, WRITE = 1 };
  Mode   mode;
  size_t byteCount;
  bool   ok;
  char  *buf;   // WRITE target (nullptr in MEASURE mode)
  size_t cap;   // capacity of buf (excluding the NUL we never need)

  explicit MqttJsonWriter(Mode m) : mode(m), byteCount(0), ok(true), buf(nullptr), cap(0) {}
  MqttJsonWriter(char *target, size_t capacity)
    : mode(WRITE), byteCount(0), ok(true), buf(target), cap(capacity) {}

  // Append len bytes from RAM source s into buf, guarding against overrun.
  bool appendRam(const char *s, size_t len) {
    if (mode == WRITE && len > 0) {
      if (byteCount + len > cap) { ok = false; return false; }
      memcpy(buf + byteCount, s, len);
    }
    byteCount += len;
    return true;
  }

  bool writeRam(const char *s) {
    if (!s) return true;
    return appendRam(s, strlen(s));
  }

  bool writeRamEscaped(const char *s) {
    if (!s) return true;
    for (const char *p = s; *p; ++p) {
      char c = *p;
      if (c == '"' || c == '\\') { if (!writeChar('\\') || !writeChar(c)) return false; }
      else if (c == '\n') { if (!writeChar('\\') || !writeChar('n')) return false; }
      else if (c == '\r') { if (!writeChar('\\') || !writeChar('r')) return false; }
      else if (c == '\t') { if (!writeChar('\\') || !writeChar('t')) return false; }
      else if ((unsigned char)c < 0x20) { char b[7]; snprintf_P(b, sizeof(b), PSTR("\\u%04x"), (unsigned)(unsigned char)c); if (!writeRam(b)) return false; }
      else { if (!writeChar(c)) return false; }
    }
    return true;
  }

  bool writeProgmem(PGM_P s) {
    if (!s) return true;
    size_t len = strlen_P(s);
    if (mode == WRITE && len > 0) {
      if (byteCount + len > cap) { ok = false; return false; }
      memcpy_P(buf + byteCount, s, len);
    }
    byteCount += len;
    return true;
  }

  bool writeChar(char c) {
    if (mode == WRITE) {
      if (byteCount + 1 > cap) { ok = false; return false; }
      buf[byteCount] = c;
    }
    byteCount += 1;
    return true;
  }

  bool writeRamN(const char *s, size_t len) {
    return appendRam(s, len);
  }
};

// ---------------------------------------------------------------------------
// Streaming discovery functions (defined in MQTTHaDiscovery.cpp)
//
// TASK-865.7: the PubSubClient &client parameter is gone. Each function now
// measures its payload, mallocs a transient buffer, fills it via MqttJsonWriter
// WRITE mode, and hands it to mqttPublishRaw() (the single publish chokepoint).
// ---------------------------------------------------------------------------
bool streamSensorDiscovery(const MqttHaSensorCfg &cfg,
                           HaDiscoveryContext &ctx);

bool streamBinarySensorDiscovery(const MqttHaBinSensorCfg &cfg,
                                 HaDiscoveryContext &ctx);

bool streamSatZoneDiscovery(HaDiscoveryContext &ctx);

bool streamClimateDiscovery(uint8_t climateIdx,
                            HaDiscoveryContext &ctx);

bool streamNumberDiscovery(HaDiscoveryContext &ctx);

// ADR-118: active gateway-override sensor (per msg id, except 27 which uses the number entity).
// label is the OTmap label (resolved by the caller, which sees OTmap).
bool streamOverrideSensorDiscovery(HaDiscoveryContext &ctx,
                                   uint8_t otid,
                                   const char* label);

// SAT enable/disable switches (boolean settings). switchIdx = 0..12, see implementation.
bool streamSatSwitchDiscovery(uint8_t switchIdx,
                              HaDiscoveryContext &ctx);

// SAT select entities (dropdowns). selectIdx = 0 (sat_heating_system) for now.
bool streamSatSelectDiscovery(uint8_t selectIdx,
                              HaDiscoveryContext &ctx);

bool streamDallasSensorDiscovery(const char *sensorAddress,
                                 HaDiscoveryContext &ctx);

bool expandAndStreamSensorSources(const MqttHaSensorCfg &cfg,
                                  HaDiscoveryContext &ctx);

// Button discovery: one entity for resetgateway (pseudo-ID 244)
bool streamButtonDiscovery(HaDiscoveryContext &ctx);

// Select discovery: GPIO and LED function selects (pseudo-ID 244)
// selectIdx: 0=gpioa, 1=gpiob, 2=leda, 3=ledb, 4=ledc, 5=ledd, 6=lede, 7=ledf
bool streamSelectDiscovery(uint8_t selectIdx,
                           HaDiscoveryContext &ctx);

// TASK-648 Task 6: topology-migration cleanup helper (defined in MQTTHaDiscovery.cpp).
// Clears (empty-retained-publishes) all discovery config topics for one OT ID
// under the STALE scheme. staleIsLegacy=true clears legacy bare-label topics;
// staleIsLegacy=false clears modern device_label topics.
// Returns the number of topics successfully cleared (0 on MQTT failure).
uint8_t clearTopologyDiscoveryForOTId(uint8_t otId,
                                      bool staleIsLegacy,
                                      const char *haPrefix,
                                      const char *nodeId,
                                      bool separateSources);

// end of MQTTstuff.h
