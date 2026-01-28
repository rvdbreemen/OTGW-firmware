# ADR-010: Multiple Concurrent Network Services

**Status:** Accepted  
**Date:** 2019-01-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW-firmware needs to serve multiple types of clients simultaneously:
- **Web browsers:** For configuration and monitoring (Web UI)
- **Home Assistant:** For MQTT-based integration
- **OTmonitor desktop app:** For advanced OpenTherm analysis
- **REST API clients:** For programmatic access
- **WebSocket clients:** For real-time message streaming

**Requirements:**
- All services must operate simultaneously without interference
- Each service has different protocol requirements
- Port assignments must not conflict
- Services must share the same underlying OpenTherm data
- Memory constraints limit number of concurrent connections

## Decision

**Run multiple network services on separate ports, each optimized for its specific use case.**

**Service architecture:**
```
Port 80/443:  HTTP Server (ESP8266WebServer)
              ├─ Web UI (HTML/CSS/JS from LittleFS)
              ├─ REST API (/api/v0/, /api/v1/, /api/v2/)
              └─ File Explorer (/FSexplorer.html)

Port 81:      WebSocket Server (WebSocketsServer)
              └─ Real-time OpenTherm message streaming

Port 25238:   Telnet Serial Bridge (WiFiServer)
              └─ OTmonitor compatibility (bidirectional serial)

Port 23:      Telnet Debug Console (TelnetStream)
              └─ Debug output and heap monitoring

MQTT:         PubSubClient (outgoing connection)
              ├─ Publish sensor values
              ├─ Receive commands
              └─ Home Assistant Auto-Discovery
```

**Port allocation rationale:**
- **80:** Standard HTTP (universal compatibility)
- **81:** WebSocket (separate to avoid HTTP/WS protocol confusion)
- **25238:** OTmonitor standard port (compatibility)
- **23:** Standard telnet port (familiar to developers)

## Alternatives Considered

### Alternative 1: Single HTTP Server for Everything
**Pros:**
- One port to configure
- Simpler firewall rules
- Single server implementation

**Cons:**
- HTTP overhead for real-time streaming
- Cannot serve OTmonitor (needs raw serial protocol)
- WebSocket upgrade on same port can cause issues
- All traffic competes for same server resources

**Why not chosen:** Different protocols have different requirements. HTTP is not suitable for real-time streaming or serial bridge.

### Alternative 2: HTTP Reverse Proxy (on-device)
**Pros:**
- Single external port
- Route internally by path

**Cons:**
- Proxy adds CPU and memory overhead
- ESP8266 too constrained for nginx/similar
- Complexity not justified
- Still need separate WebSocket port

**Why not chosen:** ESP8266 cannot run reverse proxy efficiently. Adds unnecessary complexity.

### Alternative 3: Single Port with Protocol Detection
**Pros:**
- Only one port to open
- Auto-detect HTTP/WebSocket/Telnet

**Cons:**
- Complex protocol detection logic
- Ambiguous cases
- Error-prone
- Incompatible with OTmonitor expectations
- Memory overhead for protocol parser

**Why not chosen:** Overly complex and error-prone. Well-defined ports are clearer.

### Alternative 4: mDNS Service Discovery
**Pros:**
- Clients auto-discover services
- No hardcoded ports needed

**Cons:**
- Not all networks support mDNS
- Firewall still needs port rules
- Adds complexity
- Windows requires Bonjour service

**Why not chosen:** Doesn't eliminate need for port configuration. Not universally supported.

### Alternative 5: UPnP Port Mapping
**Pros:**
- Automatic firewall configuration
- Dynamic port allocation

**Cons:**
- Security risk (UPnP vulnerabilities)
- Not all routers support UPnP
- Local network only (not needed for external access)
- Complex implementation

**Why not chosen:** Security concerns. Not needed for local network deployment model.

## Consequences

### Positive
- **Service isolation:** Each service optimized for its protocol
- **No interference:** HTTP, WebSocket, Telnet don't compete
- **OTmonitor compatibility:** Standard port 25238 works out-of-box
- **Developer friendly:** Port 23 telnet universally recognized
- **Protocol efficiency:** Each service uses optimal protocol (no HTTP overhead for streaming)
- **Clear separation:** Easy to debug individual services
- **Standard ports:** HTTP (80), Telnet (23) familiar to users

### Negative
- **Multiple ports:** Firewall must allow 4 ports (80, 81, 23, 25238)
  - Mitigation: Documentation lists all required ports
- **Memory overhead:** Each server instance consumes RAM
  - Mitigation: Lightweight server implementations
  - Measured: ~4KB total for all servers
- **Connection limits:** Must limit concurrent clients
  - Implemented: Max 3 WebSocket clients, 1 telnet client
- **Port conflict risk:** Other services might use same ports
  - Rare: Ports are well-chosen to avoid common conflicts

### Risks & Mitigation
- **Port exhaustion:** Running out of available ports
  - **Accepted:** 4 ports is reasonable; ESP8266 supports up to 8
- **Memory per connection:** Each connection consumes buffers
  - **Mitigation:** Hard limits on concurrent connections
  - **Mitigation:** Heap-aware connection rejection (v1.0.0)
- **Service starvation:** One service monopolizing CPU
  - **Mitigation:** Cooperative multitasking, quick service handlers
  - **Mitigation:** Adaptive throttling based on heap health

## Service Details

### HTTP Server (Port 80)
**Purpose:** Web UI and REST API  
**Library:** ESP8266WebServer  
**Endpoints:** 30+ routes  
**Memory:** ~512 bytes per request (streaming mode)  
**Concurrency:** 1 client at a time (sequential)

### WebSocket Server (Port 81)
**Purpose:** Real-time OpenTherm message streaming  
**Library:** WebSocketsServer  
**Protocol:** `ws://` (not `wss://`)  
**Memory:** 256 bytes × 3 clients = 768 bytes  
**Concurrency:** Max 3 clients  
**Rate limiting:** 20 msg/s (normal) → 5 msg/s (low heap)

### Serial Bridge (Port 25238)
**Purpose:** OTmonitor compatibility  
**Library:** WiFiServer (raw TCP)  
**Protocol:** Bidirectional serial passthrough  
**Memory:** ~256 bytes per connection  
**Concurrency:** 1 client at a time  
**Format:** Raw OpenTherm messages (compatible with OTmonitor)

### Debug Console (Port 23)
**Purpose:** Developer debugging and monitoring  
**Library:** TelnetStream  
**Output:** All `DebugTln()`, `DebugTf()` messages  
**Memory:** ~512 bytes  
**Concurrency:** 1 client at a time  
**Security:** No authentication (local network only)

### MQTT Client
**Purpose:** Home Assistant integration  
**Library:** PubSubClient  
**Direction:** Outgoing connection to broker  
**Memory:** ~2KB (client + buffers)  
**QoS:** 0 (at most once)  
**Reconnection:** Automatic with backoff

## Port Configuration Matrix

| Service | Port | Protocol | Concurrent Clients | Heap per Client | Purpose |
|---------|------|----------|-------------------|-----------------|---------|
| HTTP | 80 | HTTP/1.1 | 1 (sequential) | 512 bytes | Web UI, REST API |
| WebSocket | 81 | WebSocket (ws://) | 3 (max) | 256 bytes | Live streaming |
| Serial Bridge | 25238 | TCP (raw) | 1 | 256 bytes | OTmonitor |
| Debug Telnet | 23 | Telnet | 1 | 512 bytes | Debugging |
| MQTT | N/A | MQTT 3.1.1 | Outgoing | 2KB | HA integration |

**Total memory overhead:** ~5KB (all services idle)  
**Peak memory:** ~8KB (all services active)

## Implementation Patterns

**Service initialization:**
```cpp
void setup() {
  // HTTP server
  httpServer.begin();
  
  // WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Serial bridge
  OTGWserver.begin();
  OTGWserver.setNoDelay(true);
  
  // Debug telnet
  TelnetStream.begin();
  
  // MQTT client
  mqttClient.setServer(settingMqttBroker, settingMqttPort);
  mqttClient.setCallback(mqttCallback);
}
```

**Service handling in loop:**
```cpp
void loop() {
  // Handle each service
  httpServer.handleClient();        // HTTP requests
  webSocket.loop();                 // WebSocket events
  handleOTGWserver();               // Serial bridge
  mqttClient.loop();                // MQTT messages
  
  // Other tasks...
}
```

**Heap-aware connection acceptance (v1.0.0):**
```cpp
void onWebSocketEvent(uint8_t num, WStype_t type, ...) {
  if (type == WStype_CONNECTED) {
    // Check limits
    if (webSocket.connectedClients() >= 3) {
      webSocket.disconnect(num);  // Too many clients
      return;
    }
    
    // Check heap
    if (ESP.getFreeHeap() < HEAP_LOW) {
      webSocket.disconnect(num);  // Insufficient memory
      return;
    }
    
    // Accept connection
    DebugTf(PSTR("WebSocket client %d connected\r\n"), num);
  }
}
```

## Documentation Requirements

**User documentation must include:**
1. List of all ports used
2. Firewall configuration examples
3. Purpose of each service
4. How to disable unused services (if applicable)

**Network requirements:**
```
Required ports (incoming):
- TCP 80:    Web UI and REST API
- TCP 81:    WebSocket (real-time logs)
- TCP 23:    Debug telnet (optional, developers only)
- TCP 25238: OTmonitor bridge (optional, advanced users)

Required ports (outgoing):
- UDP 123:   NTP time sync
- TCP 1883:  MQTT broker (configurable port)
- UDP 5353:  mDNS (optional, for .local hostname)
```

## Related Decisions
- ADR-005: WebSocket for Real-Time Streaming (port 81 rationale)
- ADR-006: MQTT Integration Pattern (MQTT client)
- ADR-003: HTTP-Only Network Architecture (port 80 only, no 443)

## References
- Server implementations: `OTGW-firmware.ino` (setup), `restAPI.ino`, `webSocketStuff.ino`
- OTmonitor protocol: https://otgw.tclcode.com/otmonitor.html
- Network documentation: README.md, FLASH_GUIDE.md
- Port allocation: `networkStuff.h`
