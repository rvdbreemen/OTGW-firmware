/*
***************************************************************************
**  Program : otBusLiveness.h
**  Version 1.0.0
**
**  OT-bus presence decision, separated from publishing it.
**
**  The decision is a pure function of a clock and the last-seen stamps, so it
**  can be tested on the host with a driven clock (test/host/test_otBusLiveness.cpp).
**  Everything that touches MQTT stays in evaluateOTBusLiveness() in
**  OTGW-Core.ino, which is what calls this.
**
**  Header-only and inline on purpose: a function returning a struct defined in
**  an .ino trips Arduino's auto-prototype generator, the same reason
**  mqtt_configuratie.cpp lives outside the sketch.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License.
***************************************************************************
*/
#pragma once

#include <time.h>

// Seconds of silence after which a side of the bus counts as gone. The gateway
// sees traffic far more often than this when either side is alive.
#define OTBUS_PRESENCE_TIMEOUT_SEC 30

struct OtBusLiveness {
  bool bBoiler;              // boiler heard within the window
  bool bThermostat;          // thermostat heard within the window
  bool bOnline;              // either side heard, so the bus is carrying traffic
  bool bBoilerChanged;       // caller should publish boiler_connected
  bool bThermostatChanged;   // caller should publish thermostat_connected
  bool bOnlineChanged;       // caller should publish otgw_connected
};

//===========================================================================================
// evalOtBusLiveness() — decide presence and say which values the caller must publish.
//
// now, boilerLastSeen, thermostatLastSeen: epoch seconds.
// prev*: what was last published, so a change can be detected.
// forcePublish: publish all three regardless of change (first frame after boot).
// flashing: an ESP or PIC flash is running. The stream stops for longer than the
//   timeout by design during one, so the decision is frozen: previous values are
//   returned unchanged and nothing is flagged for publishing. Without this every
//   PIC update would drive the entities to false and back.
//
// Comparison is `now < lastSeen + TIMEOUT`, so a side heard exactly TIMEOUT
// seconds ago counts as gone. A lastSeen of 0 (never heard) is in the past for
// any sane clock, so a gateway that has never seen traffic reads as absent.
//===========================================================================================
inline OtBusLiveness evalOtBusLiveness(time_t now,
                                       time_t boilerLastSeen,
                                       time_t thermostatLastSeen,
                                       bool prevBoiler,
                                       bool prevThermostat,
                                       bool prevOnline,
                                       bool forcePublish,
                                       bool flashing)
{
  OtBusLiveness r;

  if (flashing) {
    r.bBoiler = prevBoiler;
    r.bThermostat = prevThermostat;
    r.bOnline = prevOnline;
    r.bBoilerChanged = false;
    r.bThermostatChanged = false;
    r.bOnlineChanged = false;
    return r;
  }

  r.bBoiler = (now < (boilerLastSeen + OTBUS_PRESENCE_TIMEOUT_SEC));
  r.bThermostat = (now < (thermostatLastSeen + OTBUS_PRESENCE_TIMEOUT_SEC));
  r.bOnline = r.bBoiler || r.bThermostat;

  r.bBoilerChanged = (r.bBoiler != prevBoiler) || forcePublish;
  r.bThermostatChanged = (r.bThermostat != prevThermostat) || forcePublish;
  r.bOnlineChanged = (r.bOnline != prevOnline) || forcePublish;

  return r;
}
