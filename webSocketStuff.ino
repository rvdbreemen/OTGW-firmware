/* 
***************************************************************************  
**  Program  : webSocketStuff.ino
**  Version  : v1.0.0-rc3
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
#include <TelnetStream.h>
#include "OTGW-Core.h"
#include "Debug.h"

extern String settingHostname;

// WebSocket server on port 81 (no built-in authentication; local network use only)
WebSocketsServer webSocket = WebSocketsServer(81);

// Track number of connected WebSocket clients
static uint8_t wsClientCount = 0;

// Track WebSocket initialization state
static bool wsInitialized = false;

// Queue for WebSocket log messages to decouple processing from serial loop
// Sized for 3-4 messages/second with processing time ~9ms/msg = up to 4 seconds of burst buffering
#define WS_LOG_QUEUE_SIZE 16 
static OTlogStruct wsLogQueue[WS_LOG_QUEUE_SIZE];
static uint8_t wsLogQueueHead = 0;
static uint8_t wsLogQueueTail = 0;
static uint8_t wsLogQueueCount = 0;

// Buffer for JSON serialization - static to avoid stack pressure
// NOTE: ArduinoJson requires a separate output buffer for serializeJson() because
// the WebSocketsServer library's broadcastTXT() API requires the entire message
// in a contiguous buffer. We cannot stream directly to WebSocket clients.
// Size: 512 bytes provides margin over max serialized JSON (~344 bytes)
#define WS_JSON_BUFFER_SIZE 512
static char wsJsonBuffer[WS_JSON_BUFFER_SIZE];

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
        IPAddress ip = webSocket.remoteIP(num);
        wsClientCount++;
        DebugTf(PSTR("WebSocket[%u] connected from %d.%d.%d.%d. Clients: %u\r\n"), 
                num, ip[0], ip[1], ip[2], ip[3], wsClientCount);
        
        // Send welcome message to newly connected client
        // Buffer sized for message prefix (33) + max hostname (32) + null terminator (1) + margin (14) = 80
        char welcomeMsg[80];
        snprintf(welcomeMsg, sizeof(welcomeMsg), "Connected to OTGW Log Stream - %s", CSTR(settingHostname));
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
// Send JSON message to all connected clients
//===========================================================================================
void sendWebSocketJSON(const char *json) {
  if (wsClientCount > 0) {
    webSocket.broadcastTXT(json);
  }
}

//===========================================================================================
// Add a log message to the WebSocket queue
//===========================================================================================
void queueWebSocketLog(const OTlogStruct& data) {
  // If no clients connected, don't bother queuing
  if (wsClientCount == 0) return;

  // Add to circular buffer
  memcpy(&wsLogQueue[wsLogQueueHead], &data, sizeof(OTlogStruct));
  wsLogQueueHead = (wsLogQueueHead + 1) % WS_LOG_QUEUE_SIZE;
  
  if (wsLogQueueCount < WS_LOG_QUEUE_SIZE) {
    wsLogQueueCount++;
  } else {
    // Buffer full, we overwrote the oldest (Head bumped into Tail)
    // Advance tail to maintain validity
    wsLogQueueTail = (wsLogQueueTail + 1) % WS_LOG_QUEUE_SIZE;
    DebugTln(F("WS Log Queue full - dropped oldest message"));
  }
}

//===========================================================================================
// Helper: Escape JSON string (handles quotes and backslashes)
// NOTE: OT strings are internally generated and shouldn't contain special chars,
// but we handle them for safety. This is a simplified escaper - only handles " and \\
//===========================================================================================
static size_t escapeJsonString(char* dest, size_t destSize, const char* src) {
  size_t written = 0;
  while (*src && written < destSize - 1) {
    if (*src == '"' || *src == '\\') {
      if (written < destSize - 2) {
        dest[written++] = '\\';
        dest[written++] = *src++;
      } else break;
    } else {
      dest[written++] = *src++;
    }
  }
  dest[written] = '\0';
  return written;
}

//===========================================================================================
// Process one message from the queue and send it
// Uses manual JSON building with snprintf for ~70% performance improvement over ArduinoJson
//===========================================================================================
void processWebSocketQueue() {
  if (wsLogQueueCount == 0 || wsClientCount == 0) return;

  // Get the oldest message from queue
  OTlogStruct* logData = &wsLogQueue[wsLogQueueTail];

  // Build JSON manually for maximum performance
  // Format: {"time":"...","source":"...","raw":"...","dir":"...","valid":"X","id":N,"label":"...","value":"..."[,"val":N][,"data":{...}]}
  char* p = wsJsonBuffer;
  char* end = wsJsonBuffer + WS_JSON_BUFFER_SIZE - 1; // Leave room for null terminator
  
  // Start object
  p += snprintf(p, end - p, "{\"time\":\"%s\",", logData->time);
  if (p >= end) goto overflow;
  
  p += snprintf(p, end - p, "\"source\":\"%s\",", logData->source);
  if (p >= end) goto overflow;
  
  p += snprintf(p, end - p, "\"raw\":\"%s\",", logData->raw);
  if (p >= end) goto overflow;
  
  p += snprintf(p, end - p, "\"dir\":\"%s\",", logData->dir);
  if (p >= end) goto overflow;
  
  p += snprintf(p, end - p, "\"valid\":\"%c\",", logData->valid);
  if (p >= end) goto overflow;
  
  p += snprintf(p, end - p, "\"id\":%u,", logData->id);
  if (p >= end) goto overflow;
  
  p += snprintf(p, end - p, "\"label\":\"%s\",", logData->label);
  if (p >= end) goto overflow;
  
  p += snprintf(p, end - p, "\"value\":\"%s\"", logData->value);
  if (p >= end) goto overflow;
  
  // Add numeric value if present
  if (logData->valType == OT_VALTYPE_F88) {
    p += snprintf(p, end - p, ",\"val\":%.2f", logData->numval.val_f88);
    if (p >= end) goto overflow;
  } else if (logData->valType == OT_VALTYPE_S16) {
    p += snprintf(p, end - p, ",\"val\":%d", logData->numval.val_s16);
    if (p >= end) goto overflow;
  } else if (logData->valType == OT_VALTYPE_U16) {
    p += snprintf(p, end - p, ",\"val\":%u", logData->numval.val_u16);
    if (p >= end) goto overflow;
  }
  
  // Add data object if present
  if (logData->data.hasData) {
    bool hasMaster = (logData->data.master[0] != '\0');
    bool hasSlave = (logData->data.slave[0] != '\0');
    bool hasExtra = (logData->data.extra[0] != '\0');
    
    if (hasMaster || hasSlave || hasExtra) {
      p += snprintf(p, end - p, ",\"data\":{");
      if (p >= end) goto overflow;
      
      bool needComma = false;
      if (hasMaster) {
        p += snprintf(p, end - p, "\"master\":\"%s\"", logData->data.master);
        if (p >= end) goto overflow;
        needComma = true;
      }
      if (hasSlave) {
        if (needComma) {
          p += snprintf(p, end - p, ",");
          if (p >= end) goto overflow;
        }
        p += snprintf(p, end - p, "\"slave\":\"%s\"", logData->data.slave);
        if (p >= end) goto overflow;
        needComma = true;
      }
      if (hasExtra) {
        if (needComma) {
          p += snprintf(p, end - p, ",");
          if (p >= end) goto overflow;
        }
        p += snprintf(p, end - p, "\"extra\":\"%s\"", logData->data.extra);
        if (p >= end) goto overflow;
      }
      
      p += snprintf(p, end - p, "}");
      if (p >= end) goto overflow;
    }
  }
  
  // Close object
  p += snprintf(p, end - p, "}");
  if (p >= end) goto overflow;
  
  // Null terminate and broadcast
  *p = '\0';
  size_t len = p - wsJsonBuffer;
  
  if (len > 0 && len < WS_JSON_BUFFER_SIZE) {
    webSocket.broadcastTXT(wsJsonBuffer, len);
  } else {
    goto overflow;
  }
  
  // Remove from queue
  wsLogQueueTail = (wsLogQueueTail + 1) % WS_LOG_QUEUE_SIZE;
  wsLogQueueCount--;
  return;
  
overflow:
  // Buffer overflow - message too large
  DebugTf(PSTR("WS: JSON buffer overflow - message dropped (buffer: %u bytes)\r\n"), 
          (unsigned int)WS_JSON_BUFFER_SIZE);
  // Still remove from queue to prevent infinite loop
  wsLogQueueTail = (wsLogQueueTail + 1) % WS_LOG_QUEUE_SIZE;
  wsLogQueueCount--;
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
  
  // Process up to 4 queued messages per loop to catch up without blocking too long
  // At 9ms/message, this is max 36ms - acceptable for main loop
  for (uint8_t i = 0; i < 4 && wsLogQueueCount > 0; i++) {
    processWebSocketQueue();
  }
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
