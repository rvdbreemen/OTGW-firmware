/*
***************************************************************************
**  Program  : MQTTtypes.h
**  Version  : v2.0.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for the MQTT subsystem (ADR-079).
**  Contents:
**    - MQTTRuntimeSection (state.mqtt    — broker connection state)
**    - MQTTSettingsSection (settings.mqtt — persisted broker config)
**
**  Requires HOME_ASSISTANT_DISCOVERY_PREFIX define from OTGW-firmware.h;
**  must therefore be included AFTER the #define block in that header.
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

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
};
