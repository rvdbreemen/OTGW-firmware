/* 
***************************************************************************  
**  Program  : webSocketStuff.ino
**  Version  : v2.0.0-alpha.291
**
**  Copyright (c) 2021-2025 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
**  WebSocket handler for streaming OpenTherm log messages to WebUI
**
**  This module provides real-time streaming of OT log messages to the WebUI
**  using WebSockets. Simplified version with direct text broadcasting.
**
**  Features:
**  - AsyncWebSocket attached to the shared port-80 AsyncWebServer at path /ws
**    (TASK-865.10, ADR-123 Phase 3): no separate TCP listener, no dedicated port, no loop poll
**  - Broadcasts log messages directly to all connected clients
**  - Minimal memory footprint
**  - Auto-cleanup of disconnected clients (cleanupClients() from a periodic timer)
**
**  Security:
**  - The WebSocket endpoint is UNAUTHENTICATED: any client on the reachable
**    network can connect to ws://<host>/ws and receive OpenTherm log messages.
**  - This is intended for use ONLY on trusted local networks. Do NOT expose
**    the /ws endpoint directly to the internet or untrusted networks.
**  - OpenTherm logs may reveal details about your heating system usage and
**    configuration. Protect network access accordingly (firewall, VLAN, etc.).
***************************************************************************
*/

#include <TelnetStream.h>
#include "debugStuff.h"
#include "webServerCompat.h"   // extern AsyncWebServer server (port 80, TASK-865.9)

// AsyncWebSocket live-log endpoint at path /ws on the shared port-80 server.
// AsyncWebSocket is NOT a server: startWebSocket() attaches it via
// server.addHandler(&otLogWs). No built-in authentication; local network use only.
AsyncWebSocket otLogWs("/ws");

// Close wrapper for prepareForReboot() in helperStuff.ino. The otLogWs global
// is defined here (not extern'd in a header) because it needs ESPAsyncWebServer
// visibility; wrapping the call keeps helperStuff.ino free of that include chain.
// Same pattern as doMqttDisconnect() in MQTTstuff.ino.
void doWebSocketClose() {
  otLogWs.closeAll();
}

// Disconnect-all wrapper for emergencyHeapRecovery() in helperStuff.ino (ADR-107
// action #1). Same scoping rationale as doWebSocketClose() above. closeAll()
// closes all connected WS clients, releasing their lwIP buffers (~2-4 KB each).
// Browsers reconnect via the graph.js auto-reconnect.
void doWebSocketDisconnectAll() {
  otLogWs.closeAll();
}

// Maximum number of simultaneous WebSocket clients
// Rationale: Each client uses ~700 bytes (256 byte buffer + overhead)
// Limiting to 3 clients prevents heap exhaustion (3 × 700 = 2100 bytes max)
#define MAX_WEBSOCKET_CLIENTS 3

// Track WebSocket initialization state. Also enforces startWebSocket()
// idempotency: the handler is attached to the persistent port-80 server exactly
// once, even though startWebSocket() is called on every WiFi/Ethernet transition.
static bool wsInitialized = false;

// Application-level keepalive tracking
static unsigned long lastKeepaliveMs = 0;
const unsigned long KEEPALIVE_INTERVAL_MS = 30000; // 30 seconds

static const uint8_t WS_BURST_CONNECTED = 1;
static const uint8_t WS_BURST_DISCONNECTED = 2;
static const uint8_t WS_BURST_REJECTED_MAX = 3;
static const uint8_t WS_BURST_REJECTED_HEAP = 4;
static const uint8_t WS_BURST_ERROR = 5;

static unsigned long wsBurstWindowStartMs = 0;
static uint8_t wsBurstConnects = 0;
static uint8_t wsBurstDisconnects = 0;
static uint8_t wsBurstMaxRejects = 0;
static uint8_t wsBurstHeapRejects = 0;
static uint8_t wsBurstErrors = 0;
static bool wsBurstLogged = false;
const unsigned long WS_BURST_WINDOW_MS = 5000;
const uint8_t WS_BURST_LOG_THRESHOLD = 3;

static void resetWebSocketBurstWindow(unsigned long now) {
  wsBurstWindowStartMs = now;
  wsBurstConnects = 0;
  wsBurstDisconnects = 0;
  wsBurstMaxRejects = 0;
  wsBurstHeapRejects = 0;
  wsBurstErrors = 0;
  wsBurstLogged = false;
}

static void noteWebSocketBurstEvent(uint8_t eventType) {
  const unsigned long now = millis();
  if ((wsBurstWindowStartMs == 0) || ((now - wsBurstWindowStartMs) > WS_BURST_WINDOW_MS)) {
    resetWebSocketBurstWindow(now);
  }

  switch (eventType) {
    case WS_BURST_CONNECTED:
      wsBurstConnects++;
      break;
    case WS_BURST_DISCONNECTED:
      wsBurstDisconnects++;
      break;
    case WS_BURST_REJECTED_MAX:
      wsBurstMaxRejects++;
      break;
    case WS_BURST_REJECTED_HEAP:
      wsBurstHeapRejects++;
      break;
    case WS_BURST_ERROR:
      wsBurstErrors++;
      break;
  }

  const uint8_t totalEvents = wsBurstConnects + wsBurstDisconnects + wsBurstMaxRejects + wsBurstHeapRejects + wsBurstErrors;
  if (!wsBurstLogged && (totalEvents >= WS_BURST_LOG_THRESHOLD)) {
    DebugTf(PSTR("[%lu] WebSocket burst window=%lums total=%u conn=%u disc=%u rejMax=%u rejHeap=%u err=%u clients=%u heap=%u maxBlk=%u\r\n"),
            now,
            WS_BURST_WINDOW_MS,
            totalEvents,
            wsBurstConnects,
            wsBurstDisconnects,
            wsBurstMaxRejects,
            wsBurstHeapRejects,
            wsBurstErrors,
            (unsigned)otLogWs.count(),
            platformFreeHeap(),
            platformMaxFreeBlock());
    wsBurstLogged = true;
  }
}

bool hasWebSocketClients() {
  return wsInitialized && (otLogWs.count() > 0);
}

//===========================================================================================
// WebSocket event handler (AsyncWebSocket AwsEventHandler signature)
//===========================================================================================
// NOTE: AsyncWebSocket maintains its own client list (otLogWs.count()), so there is
// no parallel wsClientCount to keep in sync. The new client is already in the list
// when WS_EVT_CONNECT fires (AsyncWebSocket.cpp: _clients.emplace_back() precedes
// the event dispatch), so the cap test is "count() > MAX" — count() already includes
// the just-arrived client.
void webSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                    AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_DISCONNECT:
      noteWebSocketBurstEvent(WS_BURST_DISCONNECTED);
      DebugTf(PSTR("[%lu] WebSocket[%u] disconnected. Clients: %u\r\n"),
              millis(), client->id(), (unsigned)otLogWs.count());
      break;

    case WS_EVT_CONNECT:
      {
        // Check client limit. count() already includes this new client.
        if (otLogWs.count() > MAX_WEBSOCKET_CLIENTS) {
          noteWebSocketBurstEvent(WS_BURST_REJECTED_MAX);
          DebugTf(PSTR("[%lu] WebSocket[%u]: Max clients (%u) reached, rejecting connection\r\n"),
            millis(), client->id(), MAX_WEBSOCKET_CLIENTS);
          client->close();
          return;
        }

        // Check heap health before accepting connection.
        // Use WARNING threshold to be conservative.
        if (platformFreeHeap() < HEAP_WARNING_THRESHOLD) {
          noteWebSocketBurstEvent(WS_BURST_REJECTED_HEAP);
          DebugTf(PSTR("[%lu] WebSocket[%u]: Low heap (%u bytes), rejecting connection\r\n"),
            millis(), client->id(), platformFreeHeap());
          client->close();
          return;
        }

        IPAddress ip = client->remoteIP();
        noteWebSocketBurstEvent(WS_BURST_CONNECTED);
        DebugTf(PSTR("[%lu] WebSocket[%u] connected from %d.%d.%d.%d. Clients: %u\r\n"),
          millis(), client->id(), ip[0], ip[1], ip[2], ip[3], (unsigned)otLogWs.count());
      }
      break;

    case WS_EVT_DATA:
      // Handle incoming data from client (currently not used, but available for future commands)
      DebugTf(PSTR("[%lu] WebSocket[%u] received data (%u bytes)\r\n"),
              millis(), client->id(), static_cast<unsigned>(len));
      break;

    case WS_EVT_ERROR:
      noteWebSocketBurstEvent(WS_BURST_ERROR);
      DebugTf(PSTR("[%lu] WebSocket[%u] error\r\n"), millis(), client->id());
      break;

    case WS_EVT_PING:
      // Ping/pong handled automatically by the library
      DebugTf(PSTR("[%lu] WebSocket[%u] ping\r\n"), millis(), client->id());
      break;

    case WS_EVT_PONG:
      // Ping/pong handled automatically by the library
      DebugTf(PSTR("[%lu] WebSocket[%u] pong\r\n"), millis(), client->id());
      break;
  }
}

//===========================================================================================
// Send JSON message to all connected clients
// Used only for firmware upgrade progress notifications
//===========================================================================================
void sendWebSocketJSON(const char *json) {
  if (otLogWs.count() > 0) {
    Debugf(PSTR("[%lu] WebSocket broadcast JSON (%u bytes)\r\n"),
           millis(), static_cast<unsigned>(strlen(json)));
    otLogWs.textAll(json);
  }
}

//===========================================================================================
// Start WebSocket endpoint (attach the /ws AsyncWebSocket to the port-80 server)
//===========================================================================================
// Idempotent: attaches the handler to the persistent port-80 AsyncWebServer exactly
// once. Called from setup() (OTGW-firmware.ino) and again on every WiFi/Ethernet
// transition (networkStuff.ino, Ethernet.ino). Those transitions do NOT tear down
// the AsyncWebServer (they never call startWebserver()/server.begin() again), so the
// handler survives across them — re-binding would be the bug, the no-op is correct.
// addHandler() after server.begin() is safe: begin() only starts the TCP listener;
// _handlers is consulted live per request (ESPAsyncWebServer WebServer.cpp).
void startWebSocket() {
  if (wsInitialized) return;

  otLogWs.onEvent(webSocketEvent);
  server.addHandler(&otLogWs);

  wsInitialized = true;
  Debugf(PSTR("[%lu] WebSocket endpoint attached at ws://<host>/ws (port 80)\r\n"), millis());
}

//===========================================================================================
// Periodic WebSocket housekeeping (call from the main loop / background tasks)
//===========================================================================================
// AsyncWebSocket is serviced on the AsyncTCP task — there is no per-loop library poll.
// This timer-driven helper only does the two things the library cannot do on its own:
//  1. cleanupClients() reaps closed/stale client slots, capped at MAX_WEBSOCKET_CLIENTS.
//  2. A 30s application-level keepalive keeps watchdogs fed when no OT log flows and
//     works around Safari WebSocket ping/pong quirks (ADR-025). Shape is identical to
//     the old broadcast so index.js's keepalive handling is unchanged.
void handleWebSocket() {
  if (!wsInitialized) return;

  otLogWs.cleanupClients(MAX_WEBSOCKET_CLIENTS);

  unsigned long now = millis();
  if (otLogWs.count() > 0 &&
      (now - lastKeepaliveMs) >= KEEPALIVE_INTERVAL_MS) {
    otLogWs.textAll("{\"type\":\"keepalive\"}");
    lastKeepaliveMs = now;
  }
}

//===========================================================================================
// Send log message directly to all connected WebSocket clients
// This is called from OTGW-Core.ino when a new log line is ready
// Simplified: no queue, no JSON, just direct text broadcasting.
// Heap backpressure: gated by canSendWebSocket() so the live-log throttles/blocks
// under heap pressure instead of broadcasting every OT frame unconditionally.
// (Restores parity with dev, where this gate is wired; it had never been ported to 2.0.0.)
//===========================================================================================
void sendLogToWebSocket(const char* logMessage) {
  if (hasWebSocketClients() && logMessage != nullptr && canSendWebSocket()) {
    otLogWs.textAll(logMessage);
  }
}



/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
****************************************************************************
*/
