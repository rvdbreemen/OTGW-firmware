/*
***************************************************************************
**  Program  : SATmqttPublish.cpp
**  Version  : v2.0.0-alpha.140
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Implementation of the on-change + jittered-heartbeat SAT MQTT publish
**  helpers declared in SATmqttPublish.h (ADR-111).
**
**  Re-entrancy contract: every helper updates `shadow` BEFORE calling
**  sendMQTTData. sendMQTTData internally calls feedWatchDog() which may
**  yield on ESP8266; without the pre-write, a re-entered doBackgroundTasks
**  could observe shadow.valid==false and double-publish.
**
***************************************************************************
*/
#include "SATmqttPublish.h"

#include <Arduino.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

// Forward-declare just the sendMQTTData overloads we need. We deliberately
// do NOT include OTGW-firmware.h here — that header carries global variable
// definitions (state, OTlookupitem, ...) intended for the main TU only and
// would cause multiple-definition link errors in this separate compilation
// unit.
bool sendMQTTData(const __FlashStringHelper* topic, const char* json, const bool retain);
bool sendMQTTData(const char* topic, const char* json, const bool retain);

// Forward-declare the internal helpers so the char* overloads (defined first
// in this file) can call them — the definitions live further down to keep
// related code grouped.
static inline bool     satHeartbeatDue(uint32_t nextRepublishMs);
static inline uint32_t satNextDeadline(bool wasFirstSeen);

// ---------------------------------------------------------------------------
// const char* topic overloads — body is identical to the F() overloads. We
// duplicate (rather than template) because PROGMEM topic strings should stay
// in flash on ESP8266; templating would force one shared body and either
// drag F() topics into RAM or PSTR()-decode at every call.
// ---------------------------------------------------------------------------

bool publishIfChangedF(const char* topic, float current,
                       SATShadowF& shadow, float tolerance, uint8_t decimals,
                       bool retained)
{
  const bool firstSeen    = !shadow.valid;
  const bool valueDiff    = !firstSeen && (fabsf(current - shadow.last) >= tolerance);
  const bool heartbeatDue = !firstSeen && !valueDiff && satHeartbeatDue(shadow.nextRepublishMs);
  if (!firstSeen && !valueDiff && !heartbeatDue) return false;
  shadow.last            = current;
  shadow.valid           = true;
  shadow.nextRepublishMs = satNextDeadline(firstSeen);
  char buf[16];
  dtostrf(current, 1, decimals, buf);
  sendMQTTData(topic, buf, retained);
  return true;
}

bool publishIfChangedI(const char* topic, int32_t current,
                       SATShadowI& shadow, bool retained)
{
  const bool firstSeen    = !shadow.valid;
  const bool valueDiff    = !firstSeen && (current != shadow.last);
  const bool heartbeatDue = !firstSeen && !valueDiff && satHeartbeatDue(shadow.nextRepublishMs);
  if (!firstSeen && !valueDiff && !heartbeatDue) return false;
  shadow.last            = current;
  shadow.valid           = true;
  shadow.nextRepublishMs = satNextDeadline(firstSeen);
  char buf[16];
  snprintf_P(buf, sizeof(buf), PSTR("%ld"), (long)current);
  sendMQTTData(topic, buf, retained);
  return true;
}

bool publishIfChangedB(const char* topic, bool current,
                       SATShadowB& shadow, bool retained)
{
  const bool firstSeen    = (shadow.nextRepublishMs == 0);
  const bool valueDiff    = !firstSeen && (shadow.last != (int8_t)(current ? 1 : 0));
  const bool heartbeatDue = !firstSeen && !valueDiff && satHeartbeatDue(shadow.nextRepublishMs);
  if (!firstSeen && !valueDiff && !heartbeatDue) return false;
  shadow.last            = (int8_t)(current ? 1 : 0);
  shadow.nextRepublishMs = satNextDeadline(firstSeen);
  sendMQTTData(topic, current ? "true" : "false", retained);
  return true;
}

bool publishIfChangedS(const char* topic, const char* current,
                       SATShadowS& shadow, bool retained)
{
  const bool firstSeen    = !shadow.valid;
  const bool valueDiff    = !firstSeen && (strncmp(current, shadow.last, sizeof(shadow.last)) != 0);
  const bool heartbeatDue = !firstSeen && !valueDiff && satHeartbeatDue(shadow.nextRepublishMs);
  if (!firstSeen && !valueDiff && !heartbeatDue) return false;
  strlcpy(shadow.last, current, sizeof(shadow.last));
  shadow.valid           = true;
  shadow.nextRepublishMs = satNextDeadline(firstSeen);
  sendMQTTData(topic, current, retained);
  return true;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// True when the per-topic heartbeat deadline has elapsed. The (int32_t) cast
// makes the comparison rollover-safe for any scheduling window shorter than
// ~24 days, which 11 minutes obviously is.
static inline bool satHeartbeatDue(uint32_t nextRepublishMs) {
  return (int32_t)(millis() - nextRepublishMs) >= 0;
}

// Seed the next heartbeat deadline after a publish (boot-scatter on first
// publish, normal jitter on every subsequent publish).
static inline uint32_t satNextDeadline(bool wasFirstSeen) {
  return millis() + (wasFirstSeen ? satRandomBootScatterMs()
                                  : satRandomHeartbeatMs());
}

// ---------------------------------------------------------------------------
// Float helper
// ---------------------------------------------------------------------------

bool publishIfChangedF(const __FlashStringHelper* topic, float current,
                       SATShadowF& shadow, float tolerance, uint8_t decimals,
                       bool retained)
{
  const bool firstSeen   = !shadow.valid;
  const bool valueDiff   = !firstSeen && (fabsf(current - shadow.last) >= tolerance);
  const bool heartbeatDue = !firstSeen && !valueDiff && satHeartbeatDue(shadow.nextRepublishMs);

  if (!firstSeen && !valueDiff && !heartbeatDue) return false;

  // Update shadow FIRST (re-entrancy safety — sendMQTTData may feedWatchDog/yield).
  shadow.last            = current;
  shadow.valid           = true;
  shadow.nextRepublishMs = satNextDeadline(firstSeen);

  // dtostrf matches the formatting convention used throughout satPublishMQTT().
  char buf[16];
  dtostrf(current, 1, decimals, buf);
  sendMQTTData(topic, buf, retained);
  return true;
}

// ---------------------------------------------------------------------------
// Int helper
// ---------------------------------------------------------------------------

bool publishIfChangedI(const __FlashStringHelper* topic, int32_t current,
                       SATShadowI& shadow, bool retained)
{
  const bool firstSeen    = !shadow.valid;
  const bool valueDiff    = !firstSeen && (current != shadow.last);
  const bool heartbeatDue = !firstSeen && !valueDiff && satHeartbeatDue(shadow.nextRepublishMs);

  if (!firstSeen && !valueDiff && !heartbeatDue) return false;

  shadow.last            = current;
  shadow.valid           = true;
  shadow.nextRepublishMs = satNextDeadline(firstSeen);

  char buf[16];
  snprintf_P(buf, sizeof(buf), PSTR("%ld"), (long)current);
  sendMQTTData(topic, buf, retained);
  return true;
}

// ---------------------------------------------------------------------------
// Bool helper
//
// shadow.last carries an int8_t tri-state:
//   -1 = uninitialised (zero-init would give 0, so we treat shadow with
//        nextRepublishMs==0 AND last==0 as first-seen via a separate path
//        — see below).
//    0 = false published
//    1 = true published
//
// Because BSS zero-initialises last to 0, we can't distinguish "never
// published" from "published false" purely on last. We piggy-back on
// nextRepublishMs==0 to mean "never seen yet" (since the very first publish
// always sets it to now + scatter, which is non-zero).
// ---------------------------------------------------------------------------

bool publishIfChangedB(const __FlashStringHelper* topic, bool current,
                       SATShadowB& shadow, bool retained)
{
  const bool firstSeen    = (shadow.nextRepublishMs == 0);
  const bool valueDiff    = !firstSeen && (shadow.last != (int8_t)(current ? 1 : 0));
  const bool heartbeatDue = !firstSeen && !valueDiff && satHeartbeatDue(shadow.nextRepublishMs);

  if (!firstSeen && !valueDiff && !heartbeatDue) return false;

  shadow.last            = (int8_t)(current ? 1 : 0);
  shadow.nextRepublishMs = satNextDeadline(firstSeen);

  sendMQTTData(topic, current ? "true" : "false", retained);
  return true;
}

// ---------------------------------------------------------------------------
// String helper
// ---------------------------------------------------------------------------

bool publishIfChangedS(const __FlashStringHelper* topic, const char* current,
                       SATShadowS& shadow, bool retained)
{
  const bool firstSeen    = !shadow.valid;
  const bool valueDiff    = !firstSeen && (strncmp(current, shadow.last, sizeof(shadow.last)) != 0);
  const bool heartbeatDue = !firstSeen && !valueDiff && satHeartbeatDue(shadow.nextRepublishMs);

  if (!firstSeen && !valueDiff && !heartbeatDue) return false;

  strlcpy(shadow.last, current, sizeof(shadow.last));
  shadow.valid           = true;
  shadow.nextRepublishMs = satNextDeadline(firstSeen);

  sendMQTTData(topic, current, retained);
  return true;
}

// ---------------------------------------------------------------------------
// Bool helper with custom labels (ADR-111). Behaves like publishIfChangedB
// but emits caller-supplied on/off labels — for HA binary_sensor entities
// that expect "ON"/"OFF" or "1"/"0" instead of "true"/"false".
// ---------------------------------------------------------------------------

bool publishIfChangedBStr(const __FlashStringHelper* topic, bool current,
                          SATShadowB& shadow, const char* onLabel,
                          const char* offLabel, bool retained)
{
  const bool firstSeen    = (shadow.nextRepublishMs == 0);
  const bool valueDiff    = !firstSeen && (shadow.last != (int8_t)(current ? 1 : 0));
  const bool heartbeatDue = !firstSeen && !valueDiff && satHeartbeatDue(shadow.nextRepublishMs);
  if (!firstSeen && !valueDiff && !heartbeatDue) return false;
  shadow.last            = (int8_t)(current ? 1 : 0);
  shadow.nextRepublishMs = satNextDeadline(firstSeen);
  sendMQTTData(topic, current ? onLabel : offLabel, retained);
  return true;
}

// ---------------------------------------------------------------------------
// JSON attributes blob helper. Source-change signal comes from the caller
// (typically `anyChanged |= publishIfChangedF(...)` over the blob's source
// fields); the blob itself doesn't need a value-shadow because we never want
// to publish identical JSON. `nextRepublishMs == 0` doubles as the
// "uninitialised" marker (BSS zero-init).
// ---------------------------------------------------------------------------

bool publishJsonAttrIfChanged(const __FlashStringHelper* topic, const char* json,
                              uint32_t& nextRepublishMs, bool anyChanged,
                              bool retained)
{
  const bool firstSeen    = (nextRepublishMs == 0);
  const bool heartbeatDue = !firstSeen && satHeartbeatDue(nextRepublishMs);

  if (!firstSeen && !anyChanged && !heartbeatDue) return false;

  nextRepublishMs = satNextDeadline(firstSeen);
  sendMQTTData(topic, json, retained);
  return true;
}
