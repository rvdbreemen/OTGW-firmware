/*
***************************************************************************
**  Program  : Webhooktypes.h
**  Version  : v2.0.0-alpha.35
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for outbound HTTP webhooks (ADR-079).
**  Contents:
**    - WebhookSection (settings.webhook — URLs, trigger bit, payload)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

struct WebhookSection {
  bool   bEnabled         = false;
  char   sURLon[101]      = "http://homeassistant.local:8123/api/webhook/otgw_boiler";
  char   sURLoff[101]     = "http://homeassistant.local:8123/api/webhook/otgw_boiler";
  int8_t iTriggerBit      = 1;     // Default: bit 1 = CH mode (slave: CH active)
  char   sPayload[201]    = "";    // Body template for HTTP POST; empty = HTTP GET
  char   sContentType[32] = "application/json";
};
