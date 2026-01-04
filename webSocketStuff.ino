/* 
***************************************************************************  
**  Program  : webSocketStuff.ino
**  Version  : v0.10.4-beta
**
**  Copyright (c) 2021-2025 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
**  WebSocket handler for streaming OpenTherm log messages to WebUI
**
**  This module provides real-time streaming of OT log messages to the WebUI
**  using WebSockets. It has minimal RAM impact on the ESP8266 as it only
**  sends messages without buffering them.
**
**  Features:
**  - WebSocket server on port 81 (separate from HTTP)
**  - Broadcasts log messages to all connected clients
**  - Minimal memory footprint (no server-side buffering)
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

// WebSocket server on port 81 (no built-in authentication; local network use only)
WebSocketsServer webSocket = WebSocketsServer(81);

// Track number of connected WebSocket clients
static uint8_t wsClientCount = 0;

// Track WebSocket initialization state
static bool wsInitialized = false;

//===========================================================================================
// WebSocket event handler
//===========================================================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      if (wsClientCount > 0) wsClientCount--;
      DebugTf(PSTR("WebSocket[%u] disconnected. Clients: %u\r\n"), num, wsClientCount);
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        wsClientCount++;
        DebugTf(PSTR("WebSocket[%u] connected from %d.%d.%d.%d. Clients: %u\r\n"), 
                num, ip[0], ip[1], ip[2], ip[3], wsClientCount);
        
        // Send welcome message to newly connected client
        // Buffer sized for message prefix (33) + max hostname (32) + null terminator + margin
        char welcomeMsg[80];
        snprintf(welcomeMsg, sizeof(welcomeMsg), PSTR("Connected to OTGW Log Stream - %s"), CSTR(settingHostname));
        webSocket.sendTXT(num, welcomeMsg);
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
// Start WebSocket server
//===========================================================================================
void startWebSocket() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  wsInitialized = true;
  DebugTln(F("WebSocket server started on port 81"));
}

//===========================================================================================
// Handle WebSocket events (call from main loop)
//===========================================================================================
void handleWebSocket() {
  webSocket.loop();
}

//===========================================================================================
// Send log message to all connected WebSocket clients
// This is called from OTGW-Core.ino when a new log line is ready
//===========================================================================================
void sendLogToWebSocket(const char* logMessage) {
  // Only send if WebSocket is initialized, there are connected clients, and message is valid
  if (wsInitialized && wsClientCount > 0 && logMessage != nullptr) {
    // DebugTf("Sending to WS: %s\r\n", logMessage); 
    webSocket.broadcastTXT(logMessage);
  }
}

//===========================================================================================
// Get WebSocket client count (for diagnostics)
//===========================================================================================
uint8_t getWebSocketClientCount() {
  return wsClientCount;
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
