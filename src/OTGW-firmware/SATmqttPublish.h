/*
***************************************************************************
**  Program  : SATmqttPublish.h
**  Version  : v2.0.0-beta.466
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
**  Re-entrancy (precautionary): shadow write happens before the sendMQTTData
**  call. On ESP32 (ADR-128 dropped ESP8266) feedWatchDog() no longer yields and
**  doAutoConfigure is async, so this path is not re-entered cooperatively today;
**  the ordering is kept so any future yielding path cannot double-publish a field.
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

// TASK-743: the random source diverges per platform (esp_random vs os_random).
// Use platformHardwareRandom() instead of a raw platform conditional. platform.h
// is self-contained (pragma once, only needs Arduino.h) so it is safe to pull in
// here even though SATmqttPublish.h is included early in OTGW-firmware.h.
#include <platform.h>

// ---------------------------------------------------------------------------
// Heartbeat jitter window (per ADR-111 sub-rule 3).
// ---------------------------------------------------------------------------

static constexpr uint32_t SAT_HEARTBEAT_MIN_MS = 7UL  * 60UL * 1000UL;   //  420 000
static constexpr uint32_t SAT_HEARTBEAT_MAX_MS = 11UL * 60UL * 1000UL;   //  660 000
static constexpr uint32_t SAT_BOOT_SCATTER_MAX_MS = 11UL * 60UL * 1000UL;

static inline uint32_t satRandomU32() {
  return platformHardwareRandom();  // TASK-743: esp_random / os_random behind the shim
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
// Outbox gate (TASK-1339). A helper that has something to publish skips it while
// the espMqttClient outbox holds more than SAT_MQTT_OUTBOX_LIMIT packets: the shadow
// stays as it was (the topic stays first-seen / changed / due) and the deferred flag
// is set, so satControlLoopRun() calls satPublishMQTT() again every 500 ms until a pass
// skips nothing. Bounds the first full SAT publish after boot, which queued 126
// packets (one malloc each) and took free heap from ~82 KB to 8.7-14 KB on the bench.
// ---------------------------------------------------------------------------

static constexpr uint32_t SAT_MQTT_OUTBOX_LIMIT = 24;   // TASK-1339: steady-state outbox on the bench is 0-35 packets

uint32_t mqttOutboxDepth();         // MQTTstuff.ino: packets queued in the MQTT client outbox
bool     mqttConnectPaceHold();     // MQTTstuff.ino: TASK-1757 post-connect pace window says wait
bool     satPublishDeferred();      // true when a helper skipped a publish since the last clear
void     satPublishDeferredClear(); // satPublishMQTT() clears it at the start of every pass

// ---------------------------------------------------------------------------
// MQTT pacer (ADR-265, TASK-2949.6, cut 1: the SAT shadow gate). One gate, always on, for every deferrable
// bulk publisher: mqttPaceAllows() (MQTTstuff.ino, loop task only) says yes while the outbox holds at most
// MQTT_PACE_MAX_OUTBOX packets and the largest free block is at least MQTT_PACE_MIN_BLOCK bytes. The three
// constants are defined here once for all cuts. MQTT_PACE_MAX_OUTBOX is provisional: ADR-265 step 0
// (TASK-2949.1) sets the final value from the measured cost per written packet.
// kMqttPaceOn is the cut-1 switch: false makes satOutboxFull() exactly the code before this cut.
// MQTT_PACE_MAX_WAIT_MS bounds the wait: after it, one SAT ask per 500 ms publishes without a yes, in a pass or outside one (TASK-3103).
// MQTT_PACE_MIN_BLOCK is 17408 = 16384 + 1024: 16384 is EMC_MIN_FREE_MEMORY (src/libraries/espMqttClient/src/Config.h:32), below it the
// client refuses to write (Packets/Packet.cpp:315, measure = largest free block, Helpers.h:18); the 1024 B above it is the room for
// the allocations between the pacer's yes and the client's own check (a margin, not a measurement; calibration is TASK-3103).
// Loop task only: mqttPaceAllows() and the liveness statics in SATmqttPublish.cpp are not atomic.
// ---------------------------------------------------------------------------
static constexpr bool     kMqttPaceOn           = true;
static constexpr uint32_t MQTT_PACE_MAX_OUTBOX  = 6;
static constexpr uint32_t MQTT_PACE_MIN_BLOCK   = 17408;   // bytes; 16384 (Config.h:32 EMC_MIN_FREE_MEMORY, refusal Packet.cpp:315) + 1024 margin
static constexpr uint32_t MQTT_PACE_MAX_WAIT_MS = 5000;
bool     mqttPaceAllows();          // MQTTstuff.ino: outbox and largest free block both allow a deferrable publish
bool     mqttPaceSatEnabled();      // MQTTstuff.ino: state.debug.bPaceSat, the prerelease runtime switch of the SAT gate (default true)
uint32_t satPaceNoCount();          // refusals by the pacer since boot, including those the liveness bound let through (cut 1 counter)
uint32_t satPaceLiveCount();        // only the publishes the liveness bound let through since boot
// Cut 2 (TASK-2949.7, the discovery drip in MQTTstuff.ino): kMqttPaceDripOn is its compile-time default, false = the drip
// waits on dripOutboxWait() as before the cut. The runtime switch is state.debug.bPaceDrip. The drip keeps its own liveness:
// DRIP_OUTBOX_MAX_WAITS ticks in a row, then it publishes anyway (in ticks, so slow mode waits proportionally longer).
static constexpr bool kMqttPaceDripOn = true;
uint32_t dripPaceNoCount();         // drip ticks the pacer made wait since boot (cut 2 counter)
uint32_t dripPaceLiveCount();       // drip ticks the liveness bound let through since boot
// Cut 3 (TASK-2949.8, the connect stages in MQTTstuff.ino): kMqttPaceConnOn is its compile-time default, false = connStageReady()
// decides as before the cut; the runtime switch is state.debug.bPaceConn. The 200 ms gap and the 5 s publish-anyway bound stay.
static constexpr bool kMqttPaceConnOn = true;
uint32_t connPaceNoCount();         // connect portions the pacer refused at least once since boot (counted once per portion)
uint32_t connPaceLiveCount();       // connect portions the 5 s bound let go without a yes since boot
// Cut 4 (TASK-2949.9, the stale override sweep and the hourly stats in MQTTstuff.ino): kMqttPaceTailOn is its compile-time
// default, false = the sweep waits on CONN_STAGE_MAX_OUTBOX and the stats have no outbox gate, as before the cut; the runtime
// switch is state.debug.bPaceTail. One counter pair per producer (a step refused at least once, a step the bound let go).
static constexpr bool kMqttPaceTailOn = true;
uint32_t ovrPaceNoCount();
uint32_t ovrPaceLiveCount();
uint32_t statsPaceNoCount();
uint32_t statsPaceLiveCount();

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
static constexpr float SAT_EPS_COORD       = 0.00005f;  // weather latitude/longitude, published with 4 decimals
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
  uint8_t  epoch;             // TASK-1603: g_satShadowEpoch when last published
};

struct SATShadowI {
  int32_t  last;
  uint32_t nextRepublishMs;
  bool     valid;
  uint8_t  epoch;
};

struct SATShadowB {
  int8_t   last;              // -1 = invalid, 0/1 = boolean value
  uint8_t  epoch;
  uint32_t nextRepublishMs;
};

// 24 chars covers every SAT enum-name and status-string currently emitted.
// Worst-case audit: flame_status "INSUFFICIENT_DATA" (17+1), boiler_status
// "ConditioningFlame" range (≤19+1). static_assert below enforces the floor.
struct SATShadowS {
  char     last[24];
  uint32_t nextRepublishMs;
  bool     valid;
  uint8_t  epoch;
};

static_assert(sizeof(SATShadowS::last) >= 20,
              "SATShadowS::last must hold the longest published SAT string (19+null)");

// TASK-1603 (ADR-174): a shadow whose epoch differs from g_satShadowEpoch counts as unseen: its
// helper publishes once, whatever the value, then stores the epoch. satRepublishCoreState()
// increments it when Home Assistant comes back online, so every SAT state topic (retained or
// not) is sent again on the next pass. BSS zero-init (shadow.epoch 0 == 0) leaves boot unchanged.
extern uint8_t g_satShadowEpoch;

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
//   - while the outbox is above SAT_MQTT_OUTBOX_LIMIT a due publish is skipped:
//     shadow untouched, deferred flag set, returns false (TASK-1339).
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
// sat/pv_boost_enabled, sat/weather/is_day.
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
