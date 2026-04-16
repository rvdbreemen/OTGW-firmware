// AUTO-GENERATED - DO NOT EDIT.
// Run tools/generate_mqttha_progmem.py to regenerate.
// Source: src/OTGW-firmware/data/mqttha.cfg
// Generated: 2026-04-15T20:44:24Z
//
// Declarations only — actual PROGMEM data lives in mqttha_progmem.cpp
// which Arduino compiles as a separate translation unit.  This avoids
// the Xtensa single-TU relocation explosion from embedding large PROGMEM
// data in the main sketch.cpp.

#pragma once
#include <pgmspace.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Entry descriptor — 8 bytes with natural alignment
// Fields after memcpy_P to RAM:
//   id        : uint8_t  — OT message ID
//   topicOff  : uint16_t — byte offset into mqttHaTopicPool (23758 bytes)
//   msgOff    : uint32_t — byte offset into mqttHaMsgPool   (140767 bytes)
// ---------------------------------------------------------------------------
struct MqttHaCfgEntry {
  uint8_t  id;
  uint16_t topicOff;
  uint32_t msgOff;
};
static_assert(sizeof(MqttHaCfgEntry) == 8, "MqttHaCfgEntry must be 8 bytes");

// Total number of entries in mqttHaCfgTable
constexpr uint16_t MQTT_HA_CFG_COUNT = 345;

// PROGMEM data — defined in mqttha_progmem.cpp
extern const char   mqttHaTopicPool[];
extern const char   mqttHaMsgPool[];
extern const MqttHaCfgEntry mqttHaCfgTable[345];
extern const uint16_t mqttHaCfgIndex[256];
