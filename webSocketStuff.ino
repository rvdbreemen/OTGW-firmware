/* 
***************************************************************************  
**  Program  : webSocketStuff.ino
**  Version  : v1.0.0-rc4
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
**  - WebSocket server on port 81 (separate from HTTP)
**  - Broadcasts log messages directly to all connected clients
**  - Minimal memory footprint
**  - Auto-cleanup of disconnected clients
**
**  Security:
**  - The WebSocket server is UNAUTHENTICATED: any client on the reachable
**    network can connect and receive OpenTherm log messages.
**  - This is intended for use ONLY on trusted local networks. Do NOT expose
**    port 81 directly to the internet or untrusted networks.
**  - OpenTherm logs may reveal details about your heating system usage and
**    configuration. Protect network access accordingly (firewall, VLAN, etc.).
***************************************************************************
*/

#include <WebSocketsServer.h>
#include <TelnetStream.h>
#include "Debug.h"

// WebSocket server on port 81 (no built-in authentication; local network use only)
WebSocketsServer webSocket = WebSocketsServer(81);

// Track number of connected WebSocket clients
static uint8_t wsClientCount = 0;

// Maximum number of simultaneous WebSocket clients
// Rationale: Each client uses ~700 bytes (256 byte buffer + overhead)
// Limiting to 3 clients prevents heap exhaustion (3 × 700 = 2100 bytes max)
#define MAX_WEBSOCKET_CLIENTS 3

// Track WebSocket initialization state
static bool wsInitialized = false;

// Application-level keepalive tracking
static unsigned long lastKeepaliveMs = 0;
const unsigned long KEEPALIVE_INTERVAL_MS = 30000; // 30 seconds

//===========================================================================================
// WebSocket event handler
//===========================================================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      wsClientCount = (wsClientCount > 0) ? (wsClientCount - 1) : 0;
      DebugTf(PSTR("WebSocket[%u] disconnected. Clients: %u\r\n"), num, wsClientCount);
      break;
      
    case WStype_CONNECTED:
      {
        // Check client limit before accepting connection
        if (wsClientCount >= MAX_WEBSOCKET_CLIENTS) {
          DebugTf(PSTR("WebSocket[%u]: Max clients (%u) reached, rejecting connection\r\n"), 
                  num, MAX_WEBSOCKET_CLIENTS);
          webSocket.disconnect(num);
          return;
        }
        
        // Check heap health before accepting connection
        // Use WARNING threshold to be conservative
        if (ESP.getFreeHeap() < HEAP_WARNING_THRESHOLD) {
          DebugTf(PSTR("WebSocket[%u]: Low heap (%u bytes), rejecting connection\r\n"), 
                  num, ESP.getFreeHeap());
          webSocket.disconnect(num);
          return;
        }
        
        IPAddress ip = webSocket.remoteIP(num);
        wsClientCount++;
        DebugTf(PSTR("WebSocket[%u] connected from %d.%d.%d.%d. Clients: %u\r\n"), 
                num, ip[0], ip[1], ip[2], ip[3], wsClientCount);
      }
      break;
      
    case WStype_TEXT:
      // Handle incoming text from client (currently not used, but available for future commands)
      DebugTf(PSTR("WebSocket[%u] received text: %s\r\n"), num, payload);
      break;
      
    case WStype_BIN:
      // Binary data not supported
      break;
      
    case WStype_ERROR:
      DebugTf(PSTR("WebSocket[%u] error\r\n"), num);
      break;
      
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      // Fragmented messages not used
      break;
      
    case WStype_PING:
      // Ping/pong handled automatically by library
      break;
      
    case WStype_PONG:
      // Ping/pong handled automatically by library
      break;
  }
}

//===========================================================================================
// Send JSON message to all connected clients
// Used only for firmware upgrade progress notifications
//===========================================================================================
void sendWebSocketJSON(const char *json) {
  if (wsClientCount > 0) {
    webSocket.broadcastTXT(json);
  }
}

//===========================================================================================
// Start WebSocket server
//===========================================================================================
void startWebSocket() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Enable heartbeat to keep connections alive and detect dead connections
  // Ping every 15 seconds, expect pong within 3 seconds, disconnect after 2 missed pongs
  // This prevents NAT/firewall timeout and detects stale connections
  webSocket.enableHeartbeat(15000, 3000, 2);
  
  wsInitialized = true;
  DebugTln(F("WebSocket server started on port 81 with heartbeat enabled"));
}

//===========================================================================================
// Handle WebSocket events (call from main loop)
//===========================================================================================
void handleWebSocket() {
  webSocket.loop();
  
  // Send application-level keepalive every 30 seconds
  // This ensures watchdog timers stay alive even when no OTGW log messages flow
  // Also works around Safari WebSocket ping/pong quirks
  unsigned long now = millis();
  if (wsInitialized && wsClientCount > 0 && 
      (now - lastKeepaliveMs) >= KEEPALIVE_INTERVAL_MS) {
    webSocket.broadcastTXT("{\"type\":\"keepalive\"}");
    lastKeepaliveMs = now;
  }
}

//===========================================================================================
// Send log message directly to all connected WebSocket clients
// This is called from OTGW-Core.ino when a new log line is ready
// Simplified: no queue, no JSON, just direct text broadcasting
//===========================================================================================
void sendLogToWebSocket(const char* logMessage) {
  if (wsInitialized && wsClientCount > 0 && logMessage != nullptr) {
    webSocket.broadcastTXT(logMessage);
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
