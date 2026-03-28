# WebSocket Communication Flow in OTGW Firmware

---
# METADATA
Document Title: WebSocket Communication Flow Architecture
Creation Date: 2026-02-02
Firmware Version: v1.0.0-rc6
Document Type: Architecture Documentation
Status: COMPLETE
---

## Executive Summary

**Question: Does the ESP8266 start or receive the WebSocket connection?**

**Answer: The ESP8266 STARTS (acts as the server) - it does NOT receive or initiate connections to external servers.**

The ESP8266 firmware runs a **WebSocket SERVER** that **listens** for incoming connections from web browser clients. The browser (client) initiates the connection to the ESP8266 (server).

## Connection Direction

```
Browser Client                ESP8266 Device
(Initiator)                   (Server/Listener)
    |                              |
    |  1. HTTP GET (Upgrade req)   |
    |----------------------------->|
    |                              |
    |  2. HTTP 101 Switching       |
    |<-----------------------------|
    |                              |
    |  3. WebSocket OPEN           |
    |<===========================>|
    |                              |
    |  4. Stream OT messages       |
    |<-----------------------------|
    |                              |
    |  5. Keepalive pings          |
    |<-----------------------------|
    |                              |
```

## Architecture Overview

### ESP8266 Side (Server)

**File:** `webSocketStuff.ino`

1. **Server Initialization** (during boot):
   ```cpp
   // In OTGW-firmware.ino setup():
   startWebSocket();  // Line 87
   ```

2. **Server Start Function**:
   ```cpp
   void startWebSocket() {
     webSocket.begin();              // Start listening on port 81
     webSocket.onEvent(webSocketEvent); // Register event handler
     webSocket.enableHeartbeat(15000, 3000, 2); // Ping every 15s
     wsInitialized = true;
     DebugTln(F("WebSocket server started on port 81"));
   }
   ```

3. **Server Properties**:
   - **Port:** 81 (separate from HTTP port 80)
   - **Protocol:** `ws://` (plain WebSocket, not encrypted `wss://`)
   - **Max Clients:** 3 simultaneous connections
   - **Security:** NONE - unauthenticated (local network only)
   - **Library:** WebSocketsServer from Links2004

### Browser Side (Client)

**File:** `data/index.js`

1. **Client Connection Initiation**:
   ```javascript
   // Browser initiates connection when page loads
   function initOTLogWebSocket(force) {
     const wsHost = window.location.hostname;  // ESP8266 IP/hostname
     const wsPort = 81;                        // WebSocket port
     const wsURL = 'ws://' + wsHost + ':' + wsPort + '/';
     
     // Browser creates NEW connection TO the ESP8266
     otLogWS = new WebSocket(wsURL);  // Line 996
     
     // Setup event handlers
     otLogWS.onopen = function() { /* Connected! */ };
     otLogWS.onmessage = function(event) { /* Receive data */ };
     otLogWS.onclose = function() { /* Disconnected */ };
     otLogWS.onerror = function(error) { /* Error occurred */ };
   }
   ```

2. **Client Properties**:
   - **Initiator:** Browser JavaScript code
   - **Trigger:** Page load, navigation to main page, or after flash operations
   - **Auto-reconnect:** Attempts reconnection every 5 seconds on disconnect
   - **Watchdog:** 30-second timeout, reconnects if no messages received

## Complete Connection Lifecycle

### Phase 1: Boot & Server Start (ESP8266)

```
ESP8266 Boot Sequence:
┌─────────────────────────────────────────┐
│ 1. Hardware initialization              │
│ 2. WiFi connection                      │
│ 3. HTTP server start (port 80)          │
│ 4. WebSocket server start (port 81) ◄── Server begins LISTENING
│    - Binds to port 81                   │
│    - Waits for incoming connections     │
│    - No outbound connections made       │
└─────────────────────────────────────────┘
```

**Key Code:**
```cpp
// OTGW-firmware.ino:87
startWebSocket();  // ESP8266 becomes WebSocket SERVER

// webSocketStuff.ino:132-143
void startWebSocket() {
  webSocket.begin();  // Start LISTENING on port 81
  webSocket.onEvent(webSocketEvent);
  webSocket.enableHeartbeat(15000, 3000, 2);
  wsInitialized = true;
}
```

### Phase 2: Client Connection (Browser)

```
Browser Loads Web UI:
┌─────────────────────────────────────────┐
│ 1. User navigates to http://esp-ip/    │
│ 2. Browser loads index.html            │
│ 3. Browser executes index.js           │
│ 4. JavaScript calls initOTLogWebSocket()│
│    - Builds URL: ws://esp-ip:81/       │
│    - Creates WebSocket object           │
│    - Browser INITIATES connection   ◄── Client starts connection
└─────────────────────────────────────────┘
```

**Key Code:**
```javascript
// data/index.js:996
otLogWS = new WebSocket(wsURL);  // Browser connects TO ESP8266
```

### Phase 3: WebSocket Handshake

```
TCP Handshake & HTTP Upgrade:
┌────────────────────┐         ┌────────────────────┐
│   Browser Client   │         │  ESP8266 Server    │
└────────────────────┘         └────────────────────┘
         │                              │
         │  TCP SYN                     │
         │----------------------------->│
         │                              │
         │  TCP SYN-ACK                 │
         │<-----------------------------│
         │                              │
         │  TCP ACK                     │
         │----------------------------->│
         │                              │
         │  HTTP GET /                  │
         │  Upgrade: websocket          │
         │  Sec-WebSocket-Key: ...      │
         │----------------------------->│
         │                              │ ESP validates request
         │                              │ Checks client count (<3)
         │                              │ Checks heap health
         │                              │
         │  HTTP/1.1 101 Switching      │
         │  Upgrade: websocket          │
         │  Sec-WebSocket-Accept: ...   │
         │<-----------------------------│
         │                              │
         │  === WebSocket Connected === │
         │<===========================>│
```

**ESP8266 Event Handler:**
```cpp
// webSocketStuff.ino:64-86
case WStype_CONNECTED:
  // Check client limit
  if (wsClientCount >= MAX_WEBSOCKET_CLIENTS) {
    webSocket.disconnect(num);  // Reject if too many
    return;
  }
  
  // Check heap health
  if (ESP.getFreeHeap() < HEAP_WARNING_THRESHOLD) {
    webSocket.disconnect(num);  // Reject if low memory
    return;
  }
  
  wsClientCount++;  // Accept connection
  DebugTf(PSTR("WebSocket[%u] connected from %d.%d.%d.%d\r\n"), ...);
```

### Phase 4: Active Communication

```
Real-Time Message Streaming:
┌────────────────────┐         ┌────────────────────┐
│   Browser Client   │         │  ESP8266 Server    │
└────────────────────┘         └────────────────────┘
         │                              │
         │  <- OT Message Broadcast     │
         │<-----------------------------│ Server broadcasts to all clients
         │                              │
         │  <- OT Message Broadcast     │
         │<-----------------------------│
         │                              │
         │  <- Keepalive JSON           │
         │<-----------------------------│ Every 30 seconds
         │                              │
         │  <- Ping Frame               │
         │<-----------------------------│ Every 15 seconds (heartbeat)
         │                              │
         │  Pong Frame ->               │
         │----------------------------->│ Browser responds to ping
         │                              │
```

**ESP8266 Broadcast Logic:**
```cpp
// webSocketStuff.ino:167-171
void sendLogToWebSocket(const char* logMessage) {
  if (wsInitialized && wsClientCount > 0 && logMessage != nullptr) {
    webSocket.broadcastTXT(logMessage);  // Send to ALL connected clients
  }
}

// Called from OTGW-Core.ino when OpenTherm message arrives
// Server PUSHES data to clients (client does not request)
```

**Browser Message Handler:**
```javascript
// data/index.js:1051-1087
otLogWS.onmessage = function(event) {
  resetWSWatchdog();  // Reset 30-second timeout
  
  // Handle keepalive
  if (event.data.includes('"type":"keepalive"')) {
    return;  // Don't log keepalives
  }
  
  // Process OpenTherm log message
  addLogLine(event.data);  // Display in UI
};
```

### Phase 5: Disconnection & Recovery

```
Connection Loss & Reconnection:
┌────────────────────┐         ┌────────────────────┐
│   Browser Client   │         │  ESP8266 Server    │
└────────────────────┘         └────────────────────┘
         │                              │
         │  Connection Lost (timeout,   │
         │   network issue, etc.)       │
         │  X-X-X-X-X-X-X-X-X-X-X-X-X-X│
         │                              │
         │  onclose event triggered     │
         │                              │ WStype_DISCONNECTED event
         │                              │ wsClientCount--
         │                              │
         │  Wait 5 seconds...           │
         │                              │
         │  Reconnect attempt           │
         │  TCP SYN                     │
         │----------------------------->│
         │                              │
         │  [Handshake repeats]         │
         │<===========================>│
         │                              │
         │  Connected again!            │
         │                              │
```

**Browser Auto-Reconnect:**
```javascript
// data/index.js:1017-1036
otLogWS.onclose = function() {
  console.log('OT Log WebSocket disconnected');
  updateWSStatus(false);
  
  // Automatic reconnection after 5 seconds
  if (!wsReconnectTimer) {
    let delay = isFlashing ? 1000 : 5000;  // Faster during flash
    wsReconnectTimer = setTimeout(function() { 
      initOTLogWebSocket(force);  // Re-initiate connection
    }, delay);
  }
};
```

**ESP8266 Cleanup:**
```cpp
// webSocketStuff.ino:58-60
case WStype_DISCONNECTED:
  wsClientCount = (wsClientCount > 0) ? (wsClientCount - 1) : 0;
  DebugTf(PSTR("WebSocket[%u] disconnected. Clients: %u\r\n"), num, wsClientCount);
```

## Main Loop Integration

The ESP8266 continuously processes WebSocket events in its main loop:

```cpp
// OTGW-firmware.ino:351
void loop() {
  // ... other code ...
  handleWebSocket();  // Process incoming/outgoing WebSocket traffic
  // ... other code ...
}

// webSocketStuff.ino:148-160
void handleWebSocket() {
  webSocket.loop();  // Handle all WebSocket events (accept, send, receive, etc.)
  
  // Application-level keepalive every 30 seconds
  unsigned long now = millis();
  if (wsInitialized && wsClientCount > 0 && 
      (now - lastKeepaliveMs) >= KEEPALIVE_INTERVAL_MS) {
    webSocket.broadcastTXT("{\"type\":\"keepalive\"}");
    lastKeepaliveMs = now;
  }
}
```

## Message Types & Flow

### OpenTherm Log Messages (Server → Client)

**Source:** ESP8266 receives OpenTherm messages from PIC controller via Serial

**Flow:**
1. OTGW PIC controller sends OpenTherm message via Serial
2. `OTGW-Core.ino` parses message
3. Calls `sendLogToWebSocket(logMessage)`
4. `webSocketStuff.ino` broadcasts to all connected clients
5. Browser receives and displays in UI

**Format:**
```
HH:MM:SS.mmmmmm <direction> <hex message>
Example: 14:23:45.123456 >> T80200000
```

### Keepalive Messages (Server → Client)

**Purpose:** 
- Keep connections alive through NAT/firewalls
- Reset browser watchdog timer
- Detect stale connections

**Frequency:** Every 30 seconds (application-level)

**Format:** JSON
```json
{"type":"keepalive"}
```

### Heartbeat Ping/Pong (Bidirectional)

**Purpose:** Protocol-level connection health check

**Mechanism:**
- Server sends PING frame every 15 seconds
- Client automatically responds with PONG frame
- If no PONG received within 3 seconds, retry
- After 2 missed PONGs, disconnect client

**Configured in:**
```cpp
// webSocketStuff.ino:139
webSocket.enableHeartbeat(15000, 3000, 2);
//                        ^ping  ^timeout ^retries
```

### Firmware Flash Progress (Server → Client)

**Purpose:** Real-time progress updates during firmware upgrades

**Format:** JSON with progress information

**Usage:** Only during PIC or ESP8266 firmware flash operations

## Security & Network Considerations

### No Authentication

**Design Decision:** WebSocket server has NO authentication

**Rationale:**
- Device is intended for local network use ONLY
- Trust model: All devices on local network are trusted
- Adding auth would complicate browser implementation
- See ADR-003 for network security architecture

**Security Implications:**
- Anyone on the network can connect to port 81
- Anyone can view OpenTherm messages (heating system data)
- No TLS/encryption (plain `ws://`, not `wss://`)

**Recommendations:**
- Use only on trusted local networks
- Firewall port 81 from untrusted networks
- Use VPN for remote access instead of exposing to internet

### Connection Limits

**Max Clients:** 3 simultaneous connections

**Reasons:**
- Each client uses ~700 bytes RAM (256 byte buffer + overhead)
- ESP8266 has limited RAM (~40KB available)
- 3 clients = ~2.1KB max WebSocket memory

**Enforcement:**
```cpp
// webSocketStuff.ino:66-71
if (wsClientCount >= MAX_WEBSOCKET_CLIENTS) {
  DebugTf(PSTR("Max clients (%u) reached, rejecting connection\r\n"), 
          MAX_WEBSOCKET_CLIENTS);
  webSocket.disconnect(num);  // Refuse new connection
  return;
}
```

### Heap Protection

**Check before accepting connection:**
```cpp
// webSocketStuff.ino:75-80
if (ESP.getFreeHeap() < HEAP_WARNING_THRESHOLD) {
  DebugTf(PSTR("Low heap (%u bytes), rejecting connection\r\n"), 
          ESP.getFreeHeap());
  webSocket.disconnect(num);  // Refuse if low memory
  return;
}
```

## Special Scenarios

### During Firmware Flash

**Problem:** Flash operations are timing-critical and memory-intensive

**Solution:**
```javascript
// data/index.js:60-78
function enterFlashMode() {
  flashModeActive = true;
  
  // Stop all timers
  clearInterval(timeupdate);
  clearInterval(tid);
  
  // Disconnect WebSocket completely
  disconnectOTLogWebSocket();
  
  console.log('Flash mode active - all WebSocket activity stopped');
}
```

**During flash:**
- Browser disconnects WebSocket
- ESP8266 continues running WebSocket server
- New connections rejected if heap low
- After flash completes, browser reconnects

### Safari Browser Quirks

**Issue:** Safari has WebSocket connection pool limits

**Mitigation:** See ADR-025 for Safari-specific handling during uploads

### HTTPS Reverse Proxy

**Issue:** If Web UI is accessed via HTTPS, browser blocks `ws://` (mixed content)

**Detection:**
```javascript
// data/index.js:889
const isProxied = window.location.protocol === 'https:';
if (isProxied) {
  // Disable WebSocket features
  console.log("HTTPS reverse proxy detected. WebSocket not supported.");
}
```

**Limitation:** WebSocket streaming does NOT work through HTTPS reverse proxy
- Only HTTP-based features work (REST API, static pages)
- OpenTherm log streaming unavailable

## Summary

### Who Starts the Connection?

**The BROWSER (client) initiates the connection TO the ESP8266 (server).**

The ESP8266:
1. ✅ Runs a WebSocket **SERVER** that **listens** for connections
2. ✅ Waits passively for browsers to connect
3. ❌ Does NOT initiate outbound WebSocket connections
4. ❌ Does NOT connect to external WebSocket servers

The Browser:
1. ✅ **Initiates** the connection when page loads
2. ✅ Acts as WebSocket **CLIENT**
3. ✅ Creates `new WebSocket(url)` to connect TO the ESP8266
4. ✅ Handles reconnection automatically on disconnect

### Connection Pattern

```
ESP8266 Firmware               Web Browser
================               ===========
Boot sequence                  (not running)
  ↓
Start WiFi
  ↓
Start HTTP server (port 80)
  ↓
Start WebSocket server (port 81)  ← Server LISTENING
  ↓
[WAITING FOR CONNECTIONS]      User navigates to http://esp-ip/
  ↓                              ↓
  ←─────────────────────────── Browser loads page
  ←─────────────────────────── JavaScript executes
  ←─────────────────────────── new WebSocket("ws://esp-ip:81/")
  ↓                              ↓
Accept connection              Connection established
  ↓                              ↓
Broadcast OT messages  ────────→ Display in UI
  ↓                              ↓
Send keepalives       ────────→ Reset watchdog
  ↓                              ↓
[CONTINUOUS STREAMING]         [CONTINUOUS RECEIVING]
```

## References

### Code Files
- **Server:** `webSocketStuff.ino` - ESP8266 WebSocket server implementation
- **Client:** `data/index.js` - Browser WebSocket client implementation
- **Main:** `OTGW-firmware.ino` - Server initialization and main loop
- **Core:** `OTGW-Core.ino` - OpenTherm message handling and broadcasting

### Architecture Decision Records
- **ADR-005:** WebSocket for Real-Time Streaming - Primary WebSocket architecture decision
- **ADR-003:** HTTP-Only Network Architecture - Why `ws://` not `wss://`
- **ADR-010:** Multiple Concurrent Network Services - Port allocation strategy
- **ADR-025:** Safari WebSocket Connection Management - Browser compatibility

### Documentation
- **Browser Compatibility:** `docs/reviews/2026-01-26_browser-compatibility-review/`
- **WebSocket Visual Guide:** `docs/reviews/2026-01-26_browser-compatibility-review/WEBSOCKET_VISUAL_GUIDE.md`
- **Heap Optimization:** `docs/reviews/2026-01-17_dev-rc4-analysis/HEAP_OPTIMIZATION_SUMMARY.md`

### External Resources
- **WebSocket Library:** https://github.com/Links2004/arduinoWebSockets
- **MDN WebSocket API:** https://developer.mozilla.org/en-US/docs/Web/API/WebSocket
- **WebSocket Protocol (RFC 6455):** https://tools.ietf.org/html/rfc6455

---

**Last Updated:** 2026-02-02  
**Firmware Version:** v1.0.0-rc6  
**Document Version:** 1.0
