/*
***************************************************************************
**  Program : mqttBrokerAddress.h
**  Version 1.0.0
**
**  The two broker-address decisions of handleMQTT() (TASK-1250): what to do
**  with the result of a broker name lookup, and whether to look the name up
**  again after the reconnect wait.
**
**  They are pure functions of two "is this address valid" answers, so they can
**  be tested on the host (test/host/test_mqttBrokerAddress.cpp). The lookup
**  itself, the address variables and the state machine stay in MQTTstuff.ino,
**  which is what calls this.
**
**  Header-only and inline on purpose: a function returning a struct defined in
**  an .ino trips Arduino's auto-prototype generator (see otBusLiveness.h).
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License.
***************************************************************************
*/
#pragma once

struct MqttBrokerLookup {
  bool bUseResult;   // copy the lookup result into the broker address
  bool bUsable;      // the broker address is usable afterwards: go on to connect
};

//===========================================================================================
// decideMqttBrokerLookup() — what MQTT_STATE_INIT does with a lookup result.
//
// resultValid:  the lookup returned a valid address.
// currentValid: the broker address held before the lookup is valid.
//===========================================================================================
// A failed lookup (core 2.7.4 returns INADDR_NONE, 255.255.255.255) never
// overwrites the held address: PubSubClient keeps a pointer to its text, and a
// good address from before is better than none while DNS does not answer.
inline MqttBrokerLookup decideMqttBrokerLookup(bool resultValid, bool currentValid)
{
  MqttBrokerLookup d;
  d.bUseResult = resultValid;
  d.bUsable    = resultValid || currentValid;
  return d;
}

//===========================================================================================
// mqttResolveAfterWait() — after the reconnect wait, look the broker name up
// again (back to MQTT_STATE_INIT) instead of connecting to the held address.
//
// currentValid: the broker address held now is valid.
//===========================================================================================
// Only without a usable address: a lookup can block the loop for up to 10 s
// (hostByName), and that is acceptable only while MQTT cannot connect anyway.
inline bool mqttResolveAfterWait(bool currentValid)
{
  return !currentValid;
}
