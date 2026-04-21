// MQTTstuff.h -- HA MQTT Discovery data layer
// Compact PROGMEM arrays + streaming constructor declarations
// Part of OTGW-firmware, MIT license
//
// Copyright (c) 2021-2026 Robert van den Breemen
//
// Hand-written header: enums and structs are stable across cfg changes.
// Companion files: MQTTstuff.ino (MQTT engine) and MQTTHaDiscovery.cpp
// (PROGMEM data arrays + streaming discovery constructors).

#pragma once
#include <pgmspace.h>
#include <stdint.h>
#include <PubSubClient.h>

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

struct MQTTSettingsSection {
  bool    bEnable          = true;
  bool    bSecure          = false;
  char    sBroker[65]      = "homeassistant.local";
  int16_t iBrokerPort      = 1883;
  char    sUser[41]        = "";
  char    sPasswd[41]      = "";
  char    sHaprefix[41]    = HOME_ASSISTANT_DISCOVERY_PREFIX;
  bool    bHaRebootDetect  = true;
  char    sTopTopic[41]    = "OTGW";
  // Format budget: "otgw-" (5) + up to 32-char device id + optional suffix.
  // The streaming HA discovery composer may prepend/append short fragments,
  // so keep headroom. Minimum 20 bytes is a hard lower bound.
  char    sUniqueid[41]    = "";  // Initialized in readSettings
  static_assert(sizeof(sUniqueid) >= 20, "sUniqueid must fit 'otgw-' + chipId");
  bool    bOTmessage       = false;
  uint16_t iInterval       = 0;   // MQTT publish interval in seconds (0 = publish every message)
  bool    bSeparateSources = false; // ADR-040: publish source-specific topics
  bool    bDiscoveryAutoVerify = true;  // ADR-062: daily auto-heal of retained discovery configs
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
    percent,            // "%"
    bar,                // "bar"
    l_min,              // "l/min"
    kW,                 // "kW"
    W,                  // "W"
    kWh,                // "kWh"
    uA,                 // micro-ampere
    Hz,                 // "Hz"
    rpm,                // "rpm"
    ppm,                // "ppm"
    mS,                 // "mS" (milliseconds, S0 pulse time)
    h,                  // "h" (hours)
    kW_percent,         // "kW/%" (MaxCapacity composite)
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
constexpr uint8_t MQTT_HA_FLAG_ANY_SOURCE           = 0x07;
#endif

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
// Discovery context -- runtime state passed to streaming functions
// ---------------------------------------------------------------------------
struct HaDiscoveryContext {
    const char *nodeId;
    const char *hostname;
    const char *version;
    const char *mqttPubTopic;
    const char *mqttSubTopic;
    const char *haPrefix;
    const char *manufacturer;      // Hardware manufacturer (from settings.device)
    const char *model;             // Hardware model (from settings.device)
    bool        isFirstEntity;
    // Source template expansion (set per-source iteration)
    const char *sourceSuffix;
    const char *sourceName;
    const char *sourceTopicSegment;
};

// ---------------------------------------------------------------------------
// MqttJsonWriter -- dual-mode writer for streaming JSON to MQTT
//
// In MEASURE mode: no MQTT I/O; just accumulates byte count.
// In WRITE mode: writes chunks via the global writeMqttChunk helpers.
//
// This allows the same composition function to first measure the payload
// (for PubSubClient::beginPublish) and then write it, keeping the two
// passes perfectly in sync without duplicating the JSON structure logic.
//
// Defined in the header so Arduino's auto-prototype generation knows
// the type before it generates forward declarations for functions that
// use MqttJsonWriter& parameters.
// ---------------------------------------------------------------------------

// Forward declarations for chunk writers (defined in MQTTstuff.ino)
bool writeMqttChunkExt(const char *data, size_t len);
bool writeMqttProgmemChunkExt(PGM_P data, size_t len);
bool writeMqttByteExt(uint8_t b);

struct MqttJsonWriter {
  enum Mode : uint8_t { MEASURE = 0, WRITE = 1 };
  Mode   mode;
  size_t byteCount;
  bool   ok;

  MqttJsonWriter(Mode m) : mode(m), byteCount(0), ok(true) {}

  bool writeRam(const char *s) {
    if (!s) return true;
    size_t len = strlen(s);
    byteCount += len;
    if (mode == WRITE && len > 0) {
      if (!writeMqttChunkExt(s, len)) { ok = false; return false; }
    }
    return true;
  }

  bool writeProgmem(PGM_P s) {
    if (!s) return true;
    size_t len = strlen_P(s);
    byteCount += len;
    if (mode == WRITE && len > 0) {
      if (!writeMqttProgmemChunkExt(s, len)) { ok = false; return false; }
    }
    return true;
  }

  bool writeChar(char c) {
    byteCount += 1;
    if (mode == WRITE) {
      if (!writeMqttByteExt(static_cast<uint8_t>(c))) { ok = false; return false; }
    }
    return true;
  }

  bool writeRamN(const char *s, size_t len) {
    byteCount += len;
    if (mode == WRITE && len > 0) {
      if (!writeMqttChunkExt(s, len)) { ok = false; return false; }
    }
    return true;
  }
};

// ---------------------------------------------------------------------------
// Streaming discovery functions (defined in MQTTHaDiscovery.cpp)
// ---------------------------------------------------------------------------
bool streamSensorDiscovery(PubSubClient &client,
                           const MqttHaSensorCfg &cfg,
                           HaDiscoveryContext &ctx);

bool streamBinarySensorDiscovery(PubSubClient &client,
                                 const MqttHaBinSensorCfg &cfg,
                                 HaDiscoveryContext &ctx);

bool streamClimateDiscovery(PubSubClient &client,
                            uint8_t climateIdx,
                            HaDiscoveryContext &ctx);

bool streamNumberDiscovery(PubSubClient &client,
                           HaDiscoveryContext &ctx);

// SAT enable/disable switches (boolean settings). switchIdx = 0..12, see implementation.
bool streamSatSwitchDiscovery(PubSubClient &client,
                              uint8_t switchIdx,
                              HaDiscoveryContext &ctx);

// SAT select entities (dropdowns). selectIdx = 0 (sat_heating_system) for now.
bool streamSatSelectDiscovery(PubSubClient &client,
                              uint8_t selectIdx,
                              HaDiscoveryContext &ctx);

bool streamDallasSensorDiscovery(PubSubClient &client,
                                 const char *sensorAddress,
                                 HaDiscoveryContext &ctx);

bool expandAndStreamSensorSources(PubSubClient &client,
                                  const MqttHaSensorCfg &cfg,
                                  HaDiscoveryContext &ctx);

// end of MQTTstuff.h
