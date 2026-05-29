/*
***************************************************************************
**  Program  : SATmqttPublish.h
**  Version  : v2.0.0-alpha.92
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  On-change + jittered-heartbeat MQTT publish helpers for the SAT subsystem
**  (ADR-111). Replaces unconditional per-cycle publishes in satPublishMQTT(),
**  satPressureHealthPublish(), satBLEPublishStateTopics(), and
**  weatherPublishMQTT() with four typed helpers that publish only when:
**
**    (a) the shadow is uninitialised (first-seen), OR
**    (b) the value differs from the shadow by more than `tolerance` (float)
**        or strictly (int/bool/string), OR
**    (c) the per-topic heartbeat timer has elapsed
**        (uniform random 7-11 min, jittered per topic).
**
**  Boot-scatter: the very first publish per shadow seeds the next heartbeat
**  with a uniform random offset in [0, 11 min] so the first heartbeat wave
**  is spread, not stacked into one tick. Subsequent publishes use the
**  standard [7, 11] min window.
**
**  MQTT reconnect does NOT reset shadows (ADR-111 sub-rule 5): retained
**  topics recover from broker memory, non-retained topics surface within
**  one heartbeat window (≤ 11 min).
**
**  Memory cost: ~12 B per float/int shadow, ~8 B per bool, ~32 B per
**  string. Approximately 1 KB BSS total for the full SAT topic surface.
**
**  Re-entrancy: shadow write MUST happen before the sendMQTTData call so
**  that a re-entered doBackgroundTasks (via feedWatchDog → yield in older
**  paths) cannot double-publish the same field.
**
**  See: ADR-111, ADR-073, ADR-101, ADR-052 (not reused — own contract).
**
***************************************************************************
*/
#pragma once

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#if defined(ESP32)
  #include <esp_system.h>   // esp_random()
#endif
// ESP8266: os_random() (return type 'unsigned long') is declared in osapi.h,
// which is transitively pulled in by <Arduino.h> via the ESP8266 core's
// pgmspace.h. No forward declaration needed — and adding one trips an
// ambiguous-overload (uint32_t vs unsigned long, same size but distinct types).

// ---------------------------------------------------------------------------
// Heartbeat jitter window (per ADR-111 sub-rule 3).
// ---------------------------------------------------------------------------

static constexpr uint32_t SAT_HEARTBEAT_MIN_MS = 7UL  * 60UL * 1000UL;   //  420 000
static constexpr uint32_t SAT_HEARTBEAT_MAX_MS = 11UL * 60UL * 1000UL;   //  660 000
static constexpr uint32_t SAT_BOOT_SCATTER_MAX_MS = 11UL * 60UL * 1000UL;

static inline uint32_t satRandomU32() {
#if defined(ESP32)
  return esp_random();
#else
  return (uint32_t)os_random();
#endif
}

// Uniform random in [SAT_HEARTBEAT_MIN_MS, SAT_HEARTBEAT_MAX_MS] inclusive.
// span = 240 000 ≪ UINT32_MAX, so modulo bias is below 1 ppm and acceptable.
static inline uint32_t satRandomHeartbeatMs() {
  const uint32_t span = SAT_HEARTBEAT_MAX_MS - SAT_HEARTBEAT_MIN_MS;
  return SAT_HEARTBEAT_MIN_MS + (satRandomU32() % (span + 1U));
}

// Uniform random in [0, SAT_BOOT_SCATTER_MAX_MS] inclusive. Used only for the
// SECOND-publish deadline of a freshly-seen shadow, so the first heartbeat
// wave is spread across the full 11-minute window rather than colliding into
// one doBackgroundTasks tick at boot+7min.
static inline uint32_t satRandomBootScatterMs() {
  return satRandomU32() % (SAT_BOOT_SCATTER_MAX_MS + 1U);
}

// ---------------------------------------------------------------------------
// Per-field float tolerances (per ADR-111 sub-rule 4, guideline-level).
// Tune these from field observation; off-by-one in either direction is not
// an ADR violation as long as the helper pattern itself is used.
// ---------------------------------------------------------------------------

static constexpr float SAT_EPS_TEMP        = 0.05f;     // setpoint, target, room/outside temps
static constexpr float SAT_EPS_TEMP_COARSE = 0.1f;      // heating_curve, thermal drop-rate display
static constexpr float SAT_EPS_PID_OUTPUT  = 0.5f;      // pid_output (derivative jitter)
static constexpr float SAT_EPS_PID_TERM    = 0.1f;      // pid_p, pid_i, pid_d
static constexpr float SAT_EPS_ERROR       = 0.05f;
static constexpr float SAT_EPS_KP          = 0.01f;
static constexpr float SAT_EPS_KI          = 0.00001f;
static constexpr float SAT_EPS_KD          = 0.1f;
static constexpr float SAT_EPS_DERIVATIVE  = 0.0005f;
static constexpr float SAT_EPS_FRACTION    = 0.005f;    // duty_ratio, overshoot_fraction, ...
static constexpr float SAT_EPS_PRESSURE    = 0.02f;     // bar
static constexpr float SAT_EPS_POWER       = 0.05f;     // kW
static constexpr float SAT_EPS_ENERGY      = 0.005f;    // kWh (3 decimals)
static constexpr float SAT_EPS_PV_W        = 25.0f;     // W
static constexpr float SAT_EPS_DURATION    = 1.0f;      // seconds

// ---------------------------------------------------------------------------
// Shadow structs. All fields are zero-initialised by BSS at boot, which
// means valid==false and nextRepublishMs==0 — the first invocation publishes
// (first-seen branch) and seeds nextRepublishMs to a scattered offset.
// ---------------------------------------------------------------------------

struct SATShadowF {
  float    last;
  uint32_t nextRepublishMs;
  bool     valid;
};

struct SATShadowI {
  int32_t  last;
  uint32_t nextRepublishMs;
  bool     valid;
};

struct SATShadowB {
  int8_t   last;              // -1 = invalid, 0/1 = boolean value
  uint32_t nextRepublishMs;
};

// 24 chars covers every SAT enum-name and status-string currently emitted.
// Worst-case audit: flame_status "INSUFFICIENT_DATA" (17+1), boiler_status
// "ConditioningFlame" range (≤19+1). static_assert below enforces the floor.
struct SATShadowS {
  char     last[24];
  uint32_t nextRepublishMs;
  bool     valid;
};

static_assert(sizeof(SATShadowS::last) >= 20,
              "SATShadowS::last must hold the longest published SAT string (19+null)");

// ---------------------------------------------------------------------------
// Helper API (ADR-111 sub-rule 1).
//
// Each helper:
//   - reads `current`,
//   - compares against `shadow.last` using type-specific predicate,
//   - if first-seen OR changed OR heartbeat-due:
//       * writes shadow BEFORE the sendMQTTData call (re-entrancy safety),
//       * formats the value into a stack buffer,
//       * calls sendMQTTData(topic, buf, retained),
//       * returns true.
//   - otherwise returns false without touching the broker.
//
// The return value lets a caller publish a dependent JSON-attributes blob
// (e.g. sat/pid_attributes) when any of its source fields changed — HA's
// json_attributes_topic needs to stay coherent with the individual sensors.
//
// `topic` should be the topic suffix as used by sendMQTTData (the broker
// prefix is prepended inside sendMQTTData itself).
// ---------------------------------------------------------------------------

bool publishIfChangedF(const __FlashStringHelper* topic, float current,
                       SATShadowF& shadow, float tolerance, uint8_t decimals,
                       bool retained);

bool publishIfChangedI(const __FlashStringHelper* topic, int32_t current,
                       SATShadowI& shadow, bool retained);

bool publishIfChangedB(const __FlashStringHelper* topic, bool current,
                       SATShadowB& shadow, bool retained);

bool publishIfChangedS(const __FlashStringHelper* topic, const char* current,
                       SATShadowS& shadow, bool retained);

// Same as publishIfChangedB but emits caller-supplied labels for true/false
// (e.g. "ON"/"OFF" or "1"/"0") instead of "true"/"false". Used for HA
// binary_sensor entities whose state payload convention isn't true/false:
// sat/flame_health, sat/device_health, sat/cycle_health, sat/setpoint_sync,
// sat/modulation_sync, sat/ch_sync, sat/pressure_health, sat/simulation,
// sat/auto_tune, sat/pv_boost_enabled, sat/weather/is_day.
bool publishIfChangedBStr(const __FlashStringHelper* topic, bool current,
                          SATShadowB& shadow, const char* onLabel,
                          const char* offLabel, bool retained);

// `const char*` topic overloads for runtime-built topics (e.g. sat/ble/<mac>/temp,
// sat/area/<idx>). Same semantics; just routes through the matching sendMQTTData
// overload internally. Callers are responsible for matching shadow ↔ topic
// stably across calls (typically a function-static shadow per logical topic).
bool publishIfChangedF(const char* topic, float current,
                       SATShadowF& shadow, float tolerance, uint8_t decimals,
                       bool retained);
bool publishIfChangedI(const char* topic, int32_t current,
                       SATShadowI& shadow, bool retained);
bool publishIfChangedB(const char* topic, bool current,
                       SATShadowB& shadow, bool retained);
bool publishIfChangedS(const char* topic, const char* current,
                       SATShadowS& shadow, bool retained);

// JSON-attributes blob helper. Used for HA `json_attributes_topic` payloads
// that are too large (≥ ~100 bytes) to shadow as a string. The caller passes
// `anyChanged` derived from OR'ing the return values of the publishIfChanged*
// calls for the blob's source fields. Publishes when first-seen, any source
// changed, or the per-blob heartbeat is due. The caller maintains a single
// `static uint32_t nextRepublishMs = 0;` next to the blob — BSS zero-init
// stands in for "never published". Returns true when a publish happened.
bool publishJsonAttrIfChanged(const __FlashStringHelper* topic, const char* json,
                              uint32_t& nextRepublishMs, bool anyChanged,
                              bool retained);
