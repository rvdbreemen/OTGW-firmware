/*
***************************************************************************
**  Program : mqttBrokerAddress.h
**  Version 1.0.0
**
**  The broker-address decision of handleMQTT() (TASK-1250): what to do with
**  the result of a broker name lookup in MQTT_STATE_INIT.
**
**  It is a pure function, so it can be tested on the host
**  (test/host/test_mqttBrokerAddress.cpp). The lookup itself, the address
**  variables and the state machine stay in MQTTstuff.ino, which calls this.
**
**  Header-only and inline on purpose: a function returning a struct defined in
**  an .ino trips Arduino's auto-prototype generator (see otBusLiveness.h).
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License.
***************************************************************************
*/
#pragma once

#include <string.h>

struct MqttBrokerLookup {
  bool bUseResult;   // copy the lookup result into the broker address
  bool bUsable;      // the broker address is usable afterwards: go on to connect
};

//===========================================================================================
// decideMqttBrokerLookup() — what MQTT_STATE_INIT does with a lookup result.
//
// resultValid: the lookup returned a valid address.
// heldValid:   the broker address held before the lookup is valid.
// heldFor:     the broker name that held address was looked up for ("" if none).
// name:        the broker name configured now.
//
// A failed lookup (core 2.7.4 returns INADDR_NONE, 255.255.255.255) never
// overwrites the held address: PubSubClient keeps a pointer to its text, and a
// good address from before is better than none while DNS does not answer. The
// held address only counts for the name it was looked up for, so a renamed
// broker never falls back to the previous broker's address.
//===========================================================================================
inline MqttBrokerLookup decideMqttBrokerLookup(bool resultValid, bool heldValid,
                                               const char *heldFor, const char *name)
{
  const bool heldUsable = heldValid && heldFor != nullptr && name != nullptr
                          && strcmp(heldFor, name) == 0;
  MqttBrokerLookup d;
  d.bUseResult = resultValid;
  d.bUsable    = resultValid || heldUsable;
  return d;
}
