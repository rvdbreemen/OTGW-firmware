/*
***************************************************************************
**  Program  : Webhooktypes.h
**  Version  : v2.0.0-alpha.268
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
#include <type_traits>   // std::is_trivially_copyable for the WebhookJob queue item

struct WebhookSection {
  bool   bEnabled         = false;
  char   sURLon[101]      = "http://homeassistant.local:8123/api/webhook/otgw_boiler";
  char   sURLoff[101]     = "http://homeassistant.local:8123/api/webhook/otgw_boiler";
  int8_t iTriggerBit      = 1;     // Default: bit 1 = CH mode (slave: CH active)
  char   sPayload[201]    = "";    // Body template for HTTP POST; empty = HTTP GET
  char   sContentType[32] = "application/json";
};

// ===== ADR-123 Phase-4 webhook sender task (TASK-865.13) ====================
// One fully self-contained, value-copyable HTTP send. Built loop-side (or under
// OTStateLock for the AsyncTCP test endpoint) at enqueue time, then handed to
// the dedicated webhook FreeRTOS task via a value-copy queue. The sender touches
// NOTHING but the job — no settings, no OTGWState, no mutex — so it can block on
// the HTTP round-trip without ever stalling drainOTFrameQueue or the AsyncTCP
// task. Declared here (not in webhook.ino) so the type precedes the Arduino
// auto-generated prototypes for the webhook.ino helpers. Sizes mirror the source
// buffers (sURL*[101], the CMSG_SIZE-sized payload expansion) so truncation
// behaviour is byte-for-byte unchanged from the cooperative path.
struct WebhookJob {
  bool stateOn;                       // diagnostic: which edge triggered this
  bool hasPayload;                    // true => POST with sPayloadExpanded, false => GET
  char sURL[101];                     // resolved, validated-later target URL
  char sPayloadExpanded[CMSG_SIZE];   // pre-expanded POST body ({vars} already substituted)
  char sContentType[32];              // POST Content-Type header
};
static_assert(std::is_trivially_copyable<WebhookJob>::value,
              "WebhookJob must be trivially copyable for value-copy FreeRTOS queue");
