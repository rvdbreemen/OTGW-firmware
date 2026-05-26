/*
***************************************************************************
**  Program  : SATmqttPublish.cpp
**  Version  : v2.0.0-alpha.72
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
#include "OTGW-firmware.h"   // sendMQTTData() overloads

#include <Arduino.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

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
