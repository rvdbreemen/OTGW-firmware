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
#include <ArduinoJson.h>
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
#define WS_LOG_QUEUE_SIZE 2 
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
// Process one message from the queue and send it
//===========================================================================================
void processWebSocketQueue() {
  if (wsLogQueueCount == 0 || wsClientCount == 0) return;

  // Peek/Get the oldest message
  OTlogStruct* logData = &wsLogQueue[wsLogQueueTail];

  // Use a static document to avoid stack allocation and re-allocation overhead
  // This lives in global memory (BSS/Data), not stack.
  // Capacity calculation (ArduinoJson copies strings into its memory pool):
  //   - Structure: ~136 bytes (main object + nested object overhead)
  //   - String storage: ~207 bytes (all string fields copied)
  //   - Total needed: ~343 bytes, 512 bytes provides 48% safety margin
  // Note: This is separate from wsJsonBuffer - doc holds the parsed structure,
  // wsJsonBuffer holds the serialized output string for WebSocket transmission
  static StaticJsonDocument<512> doc; 
  doc.clear();

  // Populate JSON fields
  doc["time"] = logData->time;
  doc["source"] = logData->source;
  doc["raw"] = logData->raw;
  doc["dir"] = logData->dir;
  
  char validStr[2] = { logData->valid, '\0' };
  doc["valid"] = validStr;
  
  doc["id"] = logData->id;
  doc["label"] = logData->label;
  doc["value"] = logData->value;

  // Add numeric value based on type
  if (logData->valType == OT_VALTYPE_F88) {
    doc["val"] = logData->numval.val_f88;
  } else if (logData->valType == OT_VALTYPE_S16) {
    doc["val"] = logData->numval.val_s16;
  } else if (logData->valType == OT_VALTYPE_U16) {
    doc["val"] = logData->numval.val_u16;
  }
  
  // Add data object if present
  if (logData->data.hasData) {
    JsonObject dataObj = doc.createNestedObject("data");
    if (logData->data.master[0] != '\0') {
      dataObj["master"] = logData->data.master;
    }
    if (logData->data.slave[0] != '\0') {
      dataObj["slave"] = logData->data.slave;
    }
    if (logData->data.extra[0] != '\0') {
      dataObj["extra"] = logData->data.extra;
    }
  }

  // Check if document capacity was exceeded during population
  if (doc.overflowed()) {
    DebugTf(PSTR("WS: StaticJsonDocument overflow - message may be incomplete (capacity: %u bytes)\r\n"), 
            (unsigned int)doc.capacity());
    // Continue anyway - partial data is better than nothing
  }

  // Serialize to static buffer
  size_t len = serializeJson(doc, wsJsonBuffer, WS_JSON_BUFFER_SIZE);
  
  // Broadcast (serializeJson returns 0 if buffer too small)
  if (len > 0) {
    webSocket.broadcastTXT(wsJsonBuffer, len);
  } else {
    // Buffer overflow - serializeJson returns 0 when buffer is insufficient
    // Use measureJson() to get the actual size needed for diagnostics
    size_t needed = measureJson(doc);
    DebugTf(PSTR("WS: JSON buffer overflow - message dropped (needed: %u bytes, buffer: %u bytes)\r\n"), 
            (unsigned int)needed, (unsigned int)WS_JSON_BUFFER_SIZE);
  }

  // Remove from queue
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
  
  // Process up to 2 queued messages per loop to catch up without blocking too long
  if (wsLogQueueCount > 0) {
    processWebSocketQueue();
    if (wsLogQueueCount > 0) processWebSocketQueue(); 
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
